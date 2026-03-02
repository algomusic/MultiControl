// MultiControl Rotary Encoder Example
// Demonstrates rotation tracking and push-button gestures

#include "MultiControl.h"

MultiControl encoder;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Rotary Encoder Demo ===");
  Serial.println("Rotate: position 0-100 (printed on change)");
  Serial.println("Click: single-click, double-click, hold");
  Serial.println();

  // Pin A, Pin B, Push button (0 = no button)
  encoder.setEncoderPins(15, 16, 17);
  encoder.setEncoderRange(0, 100);
  encoder.setEncoderPosition(50);  // Start at midpoint

  // Acceleration: slow turning = 1 step, fast turning = up to 5x steps
  encoder.setEncoderAccel(5.0);  // max 5x multiplier (1.0 = off)
  // Optional: encoder.setEncoderAccel(5.0, 100); // second arg = threshold ms (default 80)
  // Optional: encoder.setStepsPerDetent(2); // if your encoder has 2 edges per detent
}

void loop() {
  // readChanged() polls both rotation and button every call
  int pos = encoder.readChanged();

  // --- Rotation ---
  if (pos >= 0) {
    Serial.print("Position: ");
    Serial.println(pos);
  }

  // --- Push button gestures (all work automatically) ---
  if (encoder.wasSingleClicked()) {
    Serial.println("[CLICK] Single");
  }

  if (encoder.isDoubleClicked()) {
    Serial.println("[CLICK] Double");
  }

  if (encoder.isHeld()) {
    Serial.println("[HOLD] Triggered");
  }

  delay(1);  // ~1ms poll rate for responsive encoder tracking
}

// =============================================================================
// QUICK REFERENCE
// =============================================================================
//
// SETUP:
//   encoder.setEncoderPins(pinA, pinB, buttonPin);  // buttonPin=0 for no button
//   encoder.setEncoderRange(min, max);               // clamp position to range
//   encoder.setEncoderPosition(pos);                 // set initial position
//   encoder.setStepsPerDetent(4);                    // 4 (default), 2, or 1
//   encoder.setEncoderAccel(5.0);                    // max multiplier (1.0 = off)
//   encoder.setEncoderAccel(5.0, 100);               // with custom threshold ms
//
// READING:
//   encoder.readEncoder();    // returns current position, updates button state
//   encoder.readChanged();    // returns position if changed, -1 if unchanged
//   encoder.getEncoderPosition();  // current position without polling
//
// BUTTON (requires buttonPin in setEncoderPins):
//   encoder.isPressed()           // currently held down
//   encoder.wasSingleClicked()    // confirmed single click (after double-click window)
//   encoder.isDoubleClicked()     // double-click detected
//   encoder.isHeld()              // held past 500ms (one-shot)
//   encoder.isLongPressed()       // held past 1000ms (continuous)
//   encoder.wasHeld()             // released after hold
