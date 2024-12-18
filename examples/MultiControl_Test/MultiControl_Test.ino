// ESP32 MultiControl Test for the sProject PCB
#include "MultiControl.h"
MultiControl controlPads[14];

// for S3 LED
// for S3 LED
#include <Adafruit_NeoPixel.h>
#define PIN 47
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_RGB + NEO_KHZ800);

void updateLED(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

int s = 12; // switch pad
int t = 8; // touch pad
int b = 13; // button pad

void setup() {
  Serial.begin(115200);
  delay(1000); // give time to bring up serial monitor
  for (int i=0; i<15; i++) { // setup the GPIO pins
    controlPads[i].setPin(i+1);
  }
  pixels.setBrightness(100); // 0 - 255
  updateLED(255, 0, 0); // set up S3 LED
  Serial.println("ESP32 MiltiControl Test");
}

void loop() {
    Serial.print(" button: ");Serial.print(controlPads[b-1].isPressed());
    Serial.print(" button val: ");Serial.print(controlPads[b-1].readButton());
    Serial.print(" touched: ");Serial.print(controlPads[t-1].isTouched());
    Serial.print(" touch: ");Serial.print(controlPads[t-1].readTouch());
    Serial.print(" switch: ");Serial.print(controlPads[s-1].readSwitch());
    if (controlPads[b-1].getValue() == 0 || controlPads[t-1].getValue() > 0) { // button/touch changes LED color
      updateLED(0, 255, 0);
    } else updateLED(255, 0, 0);
    Serial.print(" pot 0: ");Serial.print(controlPads[0].readPot());
    Serial.print(" pot 1: ");Serial.print(controlPads[1].readPot());
    Serial.print(" pot 2: ");Serial.print(controlPads[2].readPot());
    Serial.print(" pot 3: ");Serial.print(controlPads[3].readPot());
    Serial.println();
  delay(249);
}
