// MultiControl multiplexed buttons test
#import "MultiControl.h"
MultiControl controlPads[8];

void setup() {
  Serial.begin(115200);
  delay(1000); // give time to bring up serial monitor
  for (int i=0; i<8; i++) { // setup the GPIO pins
    controlPads[i].setPin(13);
    controlPads[i].setMuxControlPins(34, 37, 38);
    controlPads[i].setMuxChannel(i);
    // Serial.println("chan " + String(controlPads[i].getMuxChannel()));
  }
  Serial.println("MiltiControl multipled buttons Test");
}

void loop() {
  for (int i=0; i<8; i++) {
    Serial.print(" button: ");Serial.print(controlPads[i].isPressed());
    // Serial.print(" button: ");Serial.print(controlPads[i].readMuxButton());
  }
  Serial.println();
  delay(30);
}
