// MultiControl Touch Pads Example
// Best practices for multi-pad capacitive touch on ESP32
//
// Demonstrates reliable touch detection with multiple simultaneous pads.
// Capacitive touch pads suffer from coupling — touching one pad can briefly
// disturb readings on other pads. This example shows the configuration and
// patterns that handle coupling robustly.
//
// Hardware: ESP32 or ESP32-S3 with capacitive touch-capable GPIO pins
// Each pad is a conductive surface (copper, aluminium, conductive fabric)
// connected directly to a touch-capable GPIO pin.

#include "MultiControl.h"

const int NUM_PADS = 7;
const int FIRST_PAD_PIN = 1;  // GPIO pins for pads (adjust for your board)

MultiControl pads[NUM_PADS];

// State tracking
int prevTouch[NUM_PADS];
int8_t activeVoice[NUM_PADS];  // Which "voice" each pad owns (-1 = none)
bool voiceActive[4] = {false, false, false, false};
const int MAX_VOICES = 4;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== Multi-Pad Touch Demo ===");

  // Assign pins
  for (int i = 0; i < NUM_PADS; i++) {
    pads[i].setPin(FIRST_PAD_PIN + i);
    activeVoice[i] = -1;
  }

  // --- CALIBRATION ---
  // calibrateTouch() resets the baseline and reads ~50 samples over ~200ms
  // to establish a stable reference. Call this once at startup with pads untouched.
  for (int i = 0; i < NUM_PADS; i++) {
    pads[i].calibrateTouch(50);
  }

  // --- CONFIGURATION FOR MULTI-PAD USE ---
  for (int i = 0; i < NUM_PADS; i++) {
    // Debounce: 4 reads is robust for multi-pad coupling.
    // At 3-4ms polling, this gives ~12-16ms debounce.
    // Lower values (2) are faster but allow false OFF/ON from coupling.
    pads[i].setTouchDebounceReads(4);

    // Retrigger detection: DISABLE for multi-pad environments.
    // Coupling from other pads creates dip-rise patterns that are
    // indistinguishable from real rapid re-taps. Set to 0 to disable.
    // For single-pad use, the default (15) works well.
    pads[i].setRetriggerThreshold(0);

    // Minimum hold time (default 30ms): prevents false releases from
    // coupling transients when another pad is touched. The library
    // suppresses OFF transitions within this window after touch-ON.
    // 30ms is a good balance — coupling settles in 10-20ms, and you
    // can't physically tap and release a pad in under 30ms.
    // Increase if you still see false releases; decrease for faster response.
    // pads[i].setTouchMinHold(30);  // 30ms is the default

    // Thresholds (defaults: ON=22, OFF=16): the gap provides hysteresis.
    // Increase both if you get false triggers; decrease for more sensitivity.
    // The ON threshold should always be higher than OFF.
    // pads[i].setTouchThresholds(22, 16);  // defaults shown

    prevTouch[i] = pads[i].isTouched();
  }

  Serial.println("Touch pads to trigger events. Hold + tap for polyphony.");
  Serial.println();
}

void loop() {
  static unsigned long touchTime = 0;
  unsigned long now = millis();

  // Poll touch pads at 3-4ms intervals
  if (now > touchTime) {
    touchTime = now + 3;

    for (int i = 0; i < NUM_PADS; i++) {
      int val = pads[i].isTouched();

      if (val != prevTouch[i]) {
        prevTouch[i] = val;

        if (val == 1) {
          // --- TOUCH ON ---
          int voice = allocateVoice(i);
          if (voice >= 0) {
            activeVoice[i] = voice;
            voiceActive[voice] = true;
            Serial.print("Pad ");
            Serial.print(i);
            Serial.print(" ON -> voice ");
            Serial.println(voice);
          }
        } else {
          // --- TOUCH OFF ---
          if (activeVoice[i] >= 0) {
            Serial.print("Pad ");
            Serial.print(i);
            Serial.print(" OFF <- voice ");
            Serial.println(activeVoice[i]);
            voiceActive[activeVoice[i]] = false;
            activeVoice[i] = -1;
          }
        }
      }
    }
  }
}

// Simple voice allocator: find first free voice, or steal oldest if all busy
int allocateVoice(int pad) {
  // First: look for a free voice
  for (int j = 0; j < MAX_VOICES; j++) {
    if (!voiceActive[j]) return j;
  }
  // All voices busy: steal the voice from the first other pad found
  // (In a real synth, use age tracking for last-note-priority)
  for (int j = 0; j < NUM_PADS; j++) {
    if (j != pad && activeVoice[j] >= 0) {
      int stolen = activeVoice[j];
      activeVoice[j] = -1;  // Invalidate old mapping
      Serial.print("  (stole voice ");
      Serial.print(stolen);
      Serial.print(" from pad ");
      Serial.print(j);
      Serial.println(")");
      return stolen;
    }
  }
  return -1;  // Shouldn't reach here
}

// =============================================================================
// MULTI-PAD TOUCH BEST PRACTICES
// =============================================================================
//
// PROBLEM: Capacitive coupling
//   Touching one pad briefly disturbs readings on nearby pads. This causes:
//   - False releases on held pads (delta dips below OFF threshold)
//   - False retriggers (dip-rise pattern detected as rapid re-tap)
//   - Voice accumulation (min hold delays release, eating up polyphony)
//
// SOLUTION: Three layers of protection in MultiControl
//
//   1. Baseline freeze (automatic):
//      While a pad is touched, its baseline reference is frozen.
//      Prevents coupling dips from corrupting the delta calculation.
//
//   2. Minimum hold time (default 30ms, configurable):
//      After touch-ON, OFF transitions are suppressed for this period.
//      Coupling transients settle in 10-20ms, so 30ms catches them.
//      Don't set too high — it delays real releases and can cause
//      voice accumulation in fast staccato playing.
//
//   3. Debounce (default 4 reads, configurable):
//      Requires N consecutive consistent readings before state changes.
//      At 3ms polling, 4 reads = ~12ms. Filters brief coupling glitches.
//
// RETRIGGER DETECTION:
//   The library has built-in rapid re-tap detection (wasRetriggered()).
//   This works well for SINGLE pad use, but in multi-pad environments
//   coupling noise creates false dip-rise patterns that trigger it.
//   Disable with setRetriggerThreshold(0) for multi-pad use.
//
// VOICE MANAGEMENT (application-level):
//   - Track voice-to-pad mapping (activeVoice[] pattern above)
//   - When voice stealing occurs, invalidate ALL stale pad mappings
//   - Check if a pitch is already sounding before re-triggering
//   - Use uint16_t (not uint8_t) for voice age counters to avoid
//     wrap-around after 255 notes
//
// TUNING FOR YOUR HARDWARE:
//   - Start with defaults (debounce=4, minHold=30ms, thresholds=22/16)
//   - If false triggers: increase ON threshold or debounce reads
//   - If stuck notes: decrease OFF threshold or min hold time
//   - If missed touches: decrease ON threshold (but keep > OFF threshold)
//   - Use Serial debug output to verify clean ON/OFF transitions
