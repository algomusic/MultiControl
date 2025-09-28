// MultiControl multiplexed buttons test
#import "MultiControl.h"
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
  Serial.println("MiltiControl multipled buttons Test");
  
}

void loop() {
  for (int i=0; i<24; i++) {    
    int reading = controlPads[i].readMuxButton();
    if (reading != lastReading[i]) {
      lastReading[i] = reading;
      Serial.print(" control: ");Serial.print(controlPads[i].getControl());
      Serial.print(" chan: ");Serial.print(controlPads[i].getMuxChannel());
      Serial.print(" button: ");Serial.print(reading);
      Serial.println();
    }
  } 
  
  delay(50);
}
