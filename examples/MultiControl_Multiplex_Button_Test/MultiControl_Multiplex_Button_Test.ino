// MultiControl multiplexed buttons test
#include "MultiControl.h"
MultiControl controlPads[24];
int lastReading[24];
int inputPins[] = {15, 16, 17};

void setup() {
  Serial.begin(115200);
  delay(1000); // give time to bring up serial monitor
  for (int i=0; i<24; i++) { // setup the GPIO pins
    controlPads[i].setMuxControlPins(12, 13, 14); // 34, 37, 38
    controlPads[i].setPin(inputPins[(int)(i/8)]);
    controlPads[i].setMuxChannel(i);
    
    // Serial.println("chan " + String(controlPads[i].getMuxChannel()));
    // Serial.println("read pin " + String(controlPads[i].getPin()));
    // Serial.println("ctrl pin 1 " + String(controlPads[i].getMuxControlPin(0)));
    // Serial.println("ctrl pin 2 " + String(controlPads[i].getMuxControlPin(1)));
    // Serial.println("ctrl pin 3 " + String(controlPads[i].getMuxControlPin(2)));
    
  }
  Serial.println("MultiControl Multiplexed Buttons Test");
  
}

void loop() {
  for (int i=0; i<24; i++) {
    int reading = controlPads[i].readMuxButton();

    // Report button state changes
    if (reading != lastReading[i]) {
      lastReading[i] = reading;
      Serial.print("Chan ");
      Serial.print(controlPads[i].getMuxChannel());
      Serial.print(": ");
      Serial.println(reading == 0 ? "PRESSED" : "RELEASED");
    }

    // Check for double-click
    if (controlPads[i].isDoubleClicked()) {
      Serial.print("Chan ");
      Serial.print(controlPads[i].getMuxChannel());
      Serial.println(": DOUBLE-CLICK");
    }

    // Check for hold
    if (controlPads[i].isHeld()) {
      Serial.print("Chan ");
      Serial.print(controlPads[i].getMuxChannel());
      Serial.println(": HOLD");
    }

    // Check for long-press start
    if (controlPads[i].isLongPressed() && reading == 0) {
      // Only print once when long-press starts (reading==0 means still pressed)
      static bool longPressReported[24] = {false};
      if (!longPressReported[i]) {
        Serial.print("Chan ");
        Serial.print(controlPads[i].getMuxChannel());
        Serial.println(": LONG-PRESS");
        longPressReported[i] = true;
      }
      if (reading == 1) longPressReported[i] = false;
    }
  }

  delay(10);
}
