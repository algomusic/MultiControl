// ESP32 MultiControl Test for the sProject PCB
#include "MultiControl.h"

MultiControl controlPads[14];

void setup() {
  Serial.begin(115200);
  delay(1000); // give time to bring up serial monitor
  for (int i=0; i<14; i++) { // setup the GPIO pins and controller types
    controlPads[i].setPin(i+1);
  }
  Serial.println("ESP32 MiltiControl Test");
}

void loop() {
  for (int i=0; i<7; i++) {
    controlPads[i].read(); // read the pin value
    Serial.print(controlPads[i].getPin()); // print current state per pin
    Serial.print(" button: ");Serial.print(controlPads[i].isPressed());
    Serial.print(" touch: ");Serial.print(controlPads[i].readTouch());
    Serial.print(" touched: ");Serial.print(controlPads[i].isTouched());
    Serial.print(" switch: ");Serial.print(controlPads[i].readSwitch());
    Serial.print(" pot: ");Serial.print(controlPads[i].readPot());
    Serial.println();
  }
  // some globals
  Serial.print("Button poly: ");Serial.println(multiControlAnyButtonPressed);
  Serial.print("Touch poly: ");Serial.println(multiControlAnyTouchPressed);
  delay(49);
}
