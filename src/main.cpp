#include <Arduino.h>
#include <Adafruit_Fingerprint.h>
#include <HardwareSerial.h>
#include "FingerprintManager.h"
#include "SettingsManager.h"

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
      if (match.scanResult != lastMatch.scanResult) {
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
      }
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
}

void loop() {
  Serial.println(" ");
  Serial.println(" ");
  Serial.println("Place finger on sensor");
  Serial.println("----------------------");

  doScan();

  // Validate finger
  Serial.println("Remove finger");
  if (matchFound) {
    Serial.println("Finger validated");
    matchFound = false;
  } else {
    // String newPairingCode = fingerManager.generateNewPairingCode();
    // Serial.println(newPairingCode);
    id = rand() % 1000; // Generate random ID
    Serial.print("ID: ");
    Serial.println(id);
    fingerManager.enrollFinger(id, "Test");
  }
  sleep(2);
}