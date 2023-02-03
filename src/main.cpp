#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include "FingerprintManager.h"
#include "SettingsManager.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <ESP32Ping.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <TimeLib.h>


// Replace the next variables with your SSID/Password combination
const char* ssid = "Airbox_4064";
const char* password = "";

// Add your MQTT Broker IP address, example:
const char* mqtt_server = "192.168.1.112";
const int mqtt_port = 1883;
const char* mqtt_topic = "upwork";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;

// LED Pin
const int ledPin = 4;

// LCD variables
int totalColumns = 16;
int totalRows = 2;

// LCD i2C configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);
LiquidCrystal_I2C lcd(0x27, totalColumns, totalRows);
 
char Time[ ] = "Temps: 00:00:00";
char Date[ ] = "Date: 00/00/2000";
byte last_second, second_, minute_, hour_, day_, month_;
int year_;

#if (defined(__AVR__) || defined(ESP8266)) && !defined(__AVR_ATmega2560__)
// For UNO and others without hardware serial, we must use software serial...
// pin #2 is IN from sensor (GREEN wire)
// pin #3 is OUT from arduino  (WHITE wire)
// Set up the serial port to use softwareserial..
SoftwareSerial mySerial(2, 3); // RX, TX
 
#else
// On Leonardo/M0/etc, others with hardware serial, use hardware serial!
// #0 is green wire, #1 is white
#define mySerial Serial2
 
#endif
 
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);
FingerprintManager fingerManager;
uint8_t id;
Match lastMatch;
SettingsManager settingsManager;
bool matchFound = false;

// GOOD
void scan_wifi(){
  Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
      Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN)?" ":"*");
      delay(10);
    }
  }
  Serial.println("");

  // Wait a bit before scanning again
  delay(5000);
}

// GOOD
void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  wl_status_t wifi_status = WiFi.status();
  Serial.print("WiFi status: ");
  Serial.print(wifi_status);

  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(". ");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}


void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off". 
  // Changes the output state according to the message
  if (String(topic) == mqtt_topic) {
    Serial.print("Changing output to ");
    if(messageTemp == "on"){
      Serial.println("on");
      digitalWrite(ledPin, HIGH);
    }
    else if(messageTemp == "off"){
      Serial.println("off");
      digitalWrite(ledPin, LOW);
    }
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Upwork")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe(mqtt_topic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

bool doPairing() {
  String newPairingCode = settingsManager.generateNewPairingCode();

  if (fingerManager.setPairingCode(newPairingCode)) {
    AppSettings settings = settingsManager.getAppSettings();
    settings.sensorPairingCode = newPairingCode;
    settings.sensorPairingValid = true;
    settingsManager.saveAppSettings(settings);
    Serial.println("Pairing successful.");
    return true;
  } else {
    Serial.println("Pairing failed.");
    return false;
  }
}

bool checkPairingValid() {
  AppSettings settings = settingsManager.getAppSettings();

   if (!settings.sensorPairingValid) {
     if (settings.sensorPairingCode.isEmpty()) {
       // first boot, do pairing automatically so the user does not have to do this manually
       return doPairing();
     } else {
      Serial.println("Pairing has been invalidated previously.");   
      return false;
     }
   }

  String actualSensorPairingCode = fingerManager.getPairingCode();
  //Serial.println("Awaited pairing code: " + settings.sensorPairingCode);
  //Serial.println("Actual pairing code: " + actualSensorPairingCode);

  if (actualSensorPairingCode.equals(settings.sensorPairingCode))
    return true;
  else {
    if (!actualSensorPairingCode.isEmpty()) { 
      // An empty code means there was a communication problem. So we don't have a valid code, but maybe next read will succeed and we get one again.
      // But here we just got an non-empty pairing code that was different to the awaited one. So don't expect that will change in future until repairing was done.
      // -> invalidate pairing for security reasons
      AppSettings settings = settingsManager.getAppSettings();
      settings.sensorPairingValid = false;
      settingsManager.saveAppSettings(settings);
    }
    return false;
  }
}

void doScan()
{
  Match match = fingerManager.scanFingerprint();
  
  switch(match.scanResult)
  {
    case ScanResult::noFinger:
      // standard case, occurs every iteration when no finger touchs the sensor
      if (match.scanResult != lastMatch.scanResult) {
        Serial.println("no finger");
      }
      break; 
    case ScanResult::matchFound:
      //if (match.scanResult != lastMatch.scanResult) {
        if (checkPairingValid()) {
          Serial.print("Match Found -> ID: ");
          Serial.print(match.matchId);
          Serial.print(" Name: ");
          Serial.print(match.matchName);
          Serial.print(" Confidence: ");
          Serial.println(match.matchConfidence);
        } else {
          Serial.println("Pairing invalid");
        }
        Serial.println("Match Found!");
        matchFound = true;
      //}
      delay(3000); // wait some time before next scan to let the LED blink
      break;
    case ScanResult::noMatchFound:
      Serial.print("No Match Found -> Code ");
      Serial.println(match.returnCode);
      if (match.scanResult != lastMatch.scanResult) {
        Serial.println("No Match Found!"); 
      } else {
        delay(1000); // wait some time before next scan to let the LED blink
      }
      break;
    case ScanResult::error:
      Serial.print("ScanResult Error -> Code ");
      Serial.println(match.returnCode);
      break;
  };
  lastMatch = match;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  fingerManager.connect();
  lcd.init(); 
  lcd.backlight();

  delay(10);
  scan_wifi();
  timeClient.begin(); // use to turn on and turn off LCD back light
  delay(100);
  Serial.println("Scan done");

  Serial.println();

  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  Serial.print("Connecting to ");
  Serial.println(ssid);

  setup_wifi();
  Serial.print("RRSI: ");
  Serial.println(WiFi.RSSI());
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  pinMode(ledPin, OUTPUT);

  if(Ping.ping(mqtt_server)){
    Serial.println("Ping OK");
  }else{
    Serial.println("Ping KO");
  }
}

void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;
    
    // Convert the value to a char array
    // char tempString[8];
    // dtostrf(temperature, 1, 2, tempString);
    // Serial.print("Temperature: ");
    // Serial.println(tempString);
    // client.publish("esp32/temperature", tempString);
  }
  sleep(1);

  timeClient.update(); // use to refresh LCD time

  unsigned long unix_epoch = timeClient.getEpochTime();    // Get Unix epoch time from the NTP server

  second_ = second(unix_epoch);
  if (last_second != second_) {
 
    minute_ = minute(unix_epoch);
    hour_   = hour(unix_epoch);
    day_    = day(unix_epoch);
    month_  = month(unix_epoch);
    year_   = year(unix_epoch);
 
    Time[14] = second_ % 10 + 48;
    Time[13] = second_ / 10 + 48;
    Time[11]  = minute_ % 10 + 48;
    Time[10]  = minute_ / 10 + 48;
    Time[8]  = hour_   % 10 + 48;
    Time[7]  = hour_   / 10 + 48;
 
    Date[6]  = day_   / 10 + 48;
    Date[7]  = day_   % 10 + 48;
    Date[9]  = month_  / 10 + 48;
    Date[10]  = month_  % 10 + 48;
    Date[14] = (year_   / 10) % 10 + 48;
    Date[15] = year_   % 10 % 10 + 48;
 
    Serial.println(Time);
    Serial.println(Date);
 
    lcd.setCursor(0, 0);
    lcd.print(Time);
    lcd.setCursor(0, 1);
    lcd.print(Date);
    if (second_ == 0 || second_ == 10 || second_ == 20 || second_ == 30 || second_ == 40 || second_ == 50)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Bonjour !");
        lcd.setCursor(0,1);
        lcd.print("Bienvenue");
        delay(1000);
        lcd.clear(); 
        lcd.setCursor(0, 0);
        lcd.print("Scannez doigt,");
        delay(1000);
        lcd.setCursor(0,1);
        lcd.print("pour badger.");
        delay(1000);
        lcd.clear();
    }
    last_second = second_;
  }

  finger.LEDcontrol(FINGERPRINT_LED_ON, 0, FINGERPRINT_LED_BLUE);

  Serial.println(" ");
  Serial.println(" ");
  Serial.println("Place finger on sensor");
  Serial.println("----------------------");

  doScan();

  // Validate finger
  Serial.println("Remove finger");
  if (matchFound) {
    Serial.println("Finger validated");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Fingerprint OK !");
    lcd.setCursor(0,1);
    lcd.print("Bienvenue");
    delay(1000);
    matchFound = false;

    StaticJsonDocument<256> doc;
    JsonObject root = doc.to<JsonObject>();

    root["employeeId"] = lastMatch.matchId;

    char json[128];
    int b =serializeJson(doc, json);

    Serial.print("JSON = ");
    Serial.println(json);

    client.publish(mqtt_topic, json);
    Serial.println("Empreinte envoyée");
    sleep(3);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERROR !");
    lcd.setCursor(0,1);
    lcd.print("Try again");
    delay(1000);
    // String newPairingCode = fingerManager.generateNewPairingCode();
    // Serial.println(newPairingCode);
    id = rand(); // Generate random ID
    Serial.print("ID: ");
    Serial.println(id);
    fingerManager.enrollFinger(id, "Test");
  }
}