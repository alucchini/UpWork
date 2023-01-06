#include <Arduino.h>

#define SERIAL_USB Serial
#define SERIAL_FINGER Serial2

void setup()
{
  // start serial port at 9600 bps:
  SERIAL_USB.begin(115200);
  SERIAL_FINGER.begin(115200);
}

void loop()
{
  if (SERIAL_USB.available() > 0) {
    SERIAL_FINGER.write(SERIAL_USB.read());
  }
  if (SERIAL_FINGER.available() > 0) {
    SERIAL_USB.write(SERIAL_FINGER.read());
  }
}