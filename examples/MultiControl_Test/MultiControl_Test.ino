// ESP32 MultiControl Test for the sProject PCB
#include "MultiControl.h"
MultiControl controlPads[14];

// for S3 LED
#include <FastLED.h>
#define NUM_LEDS 1
#define DATA_PIN 47 // 47 for Lolin S3 mini // 21 for Waveshare S3 pico
// #define CLOCK_PIN 13
CRGB leds[NUM_LEDS];

int s = 12;
int t = 14;
int b = 7;

void setup() {
  Serial.begin(115200);
  delay(1000); // give time to bring up serial monitor
  for (int i=0; i<14; i++) { // setup the GPIO pins
    controlPads[i].setPin(i+1);
  }
  FastLED.addLeds<SM16703, DATA_PIN, RGB>(leds, NUM_LEDS); // set up S3 LED
  Serial.println("ESP32 MiltiControl Test");
}

void loop() {
    Serial.print(" button: ");Serial.print(controlPads[b-1].isPressed());
    Serial.print(" button val: ");Serial.print(controlPads[b-1].readButton());
    Serial.print(" touched: ");Serial.print(controlPads[t-1].isTouched());
    Serial.print(" touch: ");Serial.print(controlPads[t-1].readTouch());
    Serial.print(" switch: ");Serial.print(controlPads[s-1].readSwitch());
    if (controlPads[s-1].getValue() == 0) {
      leds[0] = CRGB::Red;
    } else leds[0] = CRGB::Green;
    FastLED.show();
    Serial.print(" pot: ");Serial.print(controlPads[0].readPot());
    Serial.print(" pot: ");Serial.print(controlPads[1].readPot());
    Serial.print(" pot: ");Serial.print(controlPads[2].readPot());
    Serial.print(" pot: ");Serial.print(controlPads[3].readPot());
    Serial.println();
  // some globals
  // Serial.print(" Button poly: ");Serial.println(multiControlAnyButtonPressed);
  // Serial.print(" Touch poly: ");Serial.println(multiControlAnyTouchPressed);
  delay(249);
}
