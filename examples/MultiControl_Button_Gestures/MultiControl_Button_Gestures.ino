// MultiControl Button Gestures Example
// Demonstrates click, double-click, hold, and long-press detection

#include "MultiControl.h"

const int BUTTON_PIN = 13;
MultiControl button(BUTTON_PIN, _BUTTON);

// Optimistic single-click tracking
static bool actionDone = false;
static int counter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Button Gestures Demo ===");
  Serial.println("Single-click: OPTIMISTIC (instant) + CONSERVATIVE (400ms delay)");
  Serial.println("Double-click: Two presses within 400ms");
  Serial.println("Hold: 500ms (one-shot)  |  Long-press: 1000ms (continuous)");
  Serial.println();
}

void loop() {
  button.readButton();

  // --- OPTIMISTIC SINGLE-CLICK: instant response, undo on double-click ---
  if (button.isPressed() && !actionDone && !button.isLongPressed()) {
    counter++;
    Serial.print("[OPTIMISTIC] Instant! Counter=");
    Serial.println(counter);
    actionDone = true;
  }

  if (button.isDoubleClicked()) {
    counter--;  // undo optimistic action
    Serial.print("[DOUBLE-CLICK] Undo, then +10. Counter=");
    counter += 10;
    Serial.println(counter);
  }

  if (!button.isPressed() && !button.isClickPending()) {
    actionDone = false;
  }

  // --- CONSERVATIVE SINGLE-CLICK: waits for double-click window to expire ---
  if (button.wasSingleClicked()) {
    Serial.println("[CONSERVATIVE] Single-click confirmed");
  }

  // --- HOLD: one-shot event at 500ms ---
  if (button.isHeld()) {
    Serial.println("[HOLD] Triggered");
  }

  // --- LONG-PRESS: continuous state at 1000ms ---
  static bool longPressActive = false;
  if (button.isLongPressed() && !longPressActive) {
    Serial.println("[LONG-PRESS] Started");
    longPressActive = true;
  }
  if (!button.isLongPressed() && longPressActive) {
    Serial.println("[LONG-PRESS] Ended");
    longPressActive = false;
  }

  // --- WAS HELD: fires on release if button was held ---
  if (button.wasHeld()) {
    Serial.println("[RELEASED] After hold");
  }

  delay(10);
}

// =============================================================================
// USAGE PATTERNS
// =============================================================================
//
// OPTIMISTIC SINGLE-CLICK (instant + double-click support):
//   if (button.isPressed() && !done) { doAction(); done = true; }
//   if (button.isDoubleClicked()) { undoAction(); doDoubleAction(); }
//   if (!button.isPressed() && !button.isClickPending()) { done = false; }
//
// CONSERVATIVE SINGLE-CLICK (delayed but certain):
//   if (button.wasSingleClicked()) { doAction(); }
//   if (button.isDoubleClicked()) { doDoubleAction(); }
//
// HOLD vs CLICK:
//   if (button.isHeld()) { enterEditMode(); }
//   if (button.wasSingleClicked()) { toggleValue(); }
//
// LONG-PRESS CONTINUOUS ACTION:
//   if (button.isLongPressed()) { value++; delay(100); }
