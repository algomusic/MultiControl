# MultiControl Library - Codebase Analysis

> Last updated: 2026-03-02

## Overview

MultiControl is a single-header Arduino library (`MultiControl.h`, ~1470 lines) for ESP32 GPIO management. It handles multiple input types through a unified interface with support for banks and latching.

**License:** Creative Commons Attribution-NonCommercial-ShareAlike 4.0
**Based on:** ResponsiveAnalogRead library by Damien Clarke (2016) for pot smoothing

---

## Control Types

| Type | Constant | Code | Purpose |
|------|----------|------|---------|
| Touch | `_TOUCH` | 0 | Capacitive touch sensing (ESP32 only) |
| Potentiometer | `_POT` | 1 | Analog dial/knob reading |
| Button | `_BUTTON` | 2 | Digital button with debouncing |
| Switch | `_SWITCH` | 3 | On/off digital switch |
| Multiplexed Button | `_MUX_BUTTON` | 4 | Buttons via CD4051 multiplexer |
| Rotary Encoder | `_ENCODER` | 5 | Rotary encoder with optional push button |

> **Note:** These constants (`_TOUCH`, `_POT`, etc.) are `const static` **private** members of the `MultiControl` class and are not accessible from outside the class. Use the numeric codes directly when calling the constructor — e.g. `MultiControl knob(15, 1)` not `MultiControl knob(15, _POT)`.

---

## Dial/Potentiometer Reading

### Core Method: `readPot()`

**Return Values:**
- `0-1023`: Valid potentiometer reading
- `-1`: Below target, stationary (latched)
- `-2`: Above target, stationary (latched)
- `-3`: Floating/disconnected pin detected (erratic samples)
- `-4`: Below target, moved since last read (latched)
- `-5`: Above target, moved since last read (latched)

**Note:** Movement detection uses the pre-slew pot reading with edge snapping (values < 20 snap to 0, values > 1003 snap to 1022) to ensure consistent latch tracking despite ADC noise at extremes.

### Smoothing Pipeline

1. **4-sample median** - Takes 4 readings with 10µs settling time, uses network sort (5 swaps) to find median for noise rejection
2. **Floating pin detection** - If sample spread > `_maxSampleSpread` (50), returns -3
3. **Edge locking** - Locks to 0 or 1023 if all samples near extremes
4. **Responsive smoothing** - EMA-based adaptive algorithm from ResponsiveAnalogRead
5. **Hysteresis** - Suppresses changes smaller than `_potHysteresis` (except near edges)
6. **Slewing** - Smooths rapid transitions with 0.5 multiplier
7. **Bank checking** - Handles preset values with latching behavior

### Tunable Parameters

| Parameter | Default | Setter Method | Purpose |
|-----------|---------|---------------|---------|
| `snapMultiplier` | 0.05 | `setSnapMultiplier(float)` | Response speed (0.0-1.0), lower = more smoothing |
| `activityThreshold` | 4.0 | `setActivityThreshold(float)` | Sleep mode threshold, higher = more aggressive |
| `_potHysteresis` | 3 | `setPotHysteresis(int)` | Min change to report new value |
| `_maxSampleSpread` | 50 | (internal) | Floating pin detection tolerance |
| `sleepEnable` | true | `setSleepEnable(bool)` | Enable sleep mode to reduce jitter |

### Variant: `readPotChanged()`
Combines `readPot()` with change detection - returns `-1` if value unchanged, otherwise the new value.

---

## Rotary Encoder

### Setup

```cpp
encoder.setEncoderPins(pinA, pinB, buttonPin);  // buttonPin=0 for no button
encoder.setEncoderRange(min, max);               // clamp position to range
encoder.setEncoderPosition(pos);                 // set initial position
encoder.setStepsPerDetent(4);                    // 4 (default), 2, or 1
encoder.setEncoderAccel(5.0);                    // acceleration multiplier (1.0 = off)
encoder.setEncoderAccel(5.0, 100);               // with custom threshold in ms
```

### Reading

- `readEncoder()` — Gray code state machine for rotation + button state machine if configured. Returns absolute position.
- `readChanged()` — returns position if changed, -1 if unchanged (also updates button state machine).
- `getEncoderPosition()` — current position without polling hardware.

### Gray Code Decoding

Uses a 16-entry lookup table indexed by `(prevState << 2 | newState)` to decode direction from 2-bit Gray code. Sub-detent edges accumulate in `_encAccum`; a full detent (`_encStepsPerDetent` edges, default 4) triggers a position change.

### Acceleration

When enabled via `setEncoderAccel(factor, thresholdMs)`, the time between completed detents is measured. If the interval falls below `thresholdMs` (default 80ms), the step size scales linearly from 1 up to `factor`. Slow turning always advances by 1.

| Parameter | Default | Purpose |
|-----------|---------|---------|
| `_encAccelFactor` | 5.0 | Max multiplier at full speed |
| `_encAccelThreshold` | 80 | ms — intervals below this trigger acceleration |

### Push Button

When a button pin is provided to `setEncoderPins()`, the encoder runs a private `readEncoderButton()` state machine (identical logic to `readButton()` but reads from the encoder's button pin). All existing gesture queries work automatically:

- `isPressed()`, `wasSingleClicked()`, `isDoubleClicked()`
- `isHeld()`, `isLongPressed()`, `wasHeld()`

---

## Bank System

Each control can store values across multiple banks (presets):

```cpp
void initBanks(int numBanks)           // Initialize N banks
void setBank(uint8_t bank)             // Switch to bank
int getBank()                          // Get current bank
void setBankValue(int bank, int val)   // Set specific bank value
int getBankValue(int bank)             // Get specific bank value
void setCurrentBankValue(int val)      // Set current bank value
```

### Latching Behavior

When switching banks, pot values are "latched" (frozen) until the physical pot position crosses the stored bank value. This prevents sudden value jumps.

```cpp
void setLatchEnabled(bool enabled)     // Enable/disable latching
bool isLatchEnabled()
void releaseLatch()                    // Manually release latch
bool isLatched()                       // Currently latching?
```

---

## Public API Reference

### Reading Methods
```cpp
int read()                    // Read any control type
int readChanged()             // Return -1 if unchanged
int readPot()                 // Potentiometer (0-1023)
int readPotChanged()          // Pot with change detection
int readButton()              // Button state
int readSwitch()              // Switch state
int readTouch()               // Touch/capacitive (ESP32)
int readMuxButton()           // Multiplexed button
int readEncoder()             // Encoder position (updates button too)
```

### Status Methods
```cpp
int getValue()                // Get current value for current bank
bool isPressed()              // Button pressed?
bool isTouched()              // Touch pad touched? (ESP32)
bool wasRetriggered()         // Rapid retrigger detected? (read-once flag)
bool isDoubleClicked()        // Double-click detected?
bool isHeld()                 // Hold threshold reached?
bool wasHeld()                // Was held on release?
bool isSwitchedOn()           // Switch in ON position?
bool isLatched()              // Bank latching active?
```

### Configuration Methods
```cpp
void setPin(uint8_t pin)
void setControl(uint8_t type)
void setPotHysteresis(int val)
void setActivityThreshold(float val)
void setSnapMultiplier(float val)
void setSleepEnable(bool enabled)
void setDebounceTime(unsigned long ms)
void setHoldTime(unsigned long ms)
void setDoubleClickTime(unsigned long ms)
void setTouchThresholds(int16_t on, int16_t off)
void setTouchDebounceReads(uint8_t reads)
void setRetriggerThreshold(int16_t threshold)
void setTouchMinHold(uint16_t ms)
void calibrateTouch(int readings = 50)
void resetTouchBaseline()
void resetRetriggerState()
```

### Encoder Methods
```cpp
void setEncoderPins(uint8_t pinA, uint8_t pinB, uint8_t buttonPin = 0)
void setEncoderRange(int minVal, int maxVal)
void setEncoderPosition(int pos)
int getEncoderPosition()
void setStepsPerDetent(int8_t steps)
void setEncoderAccel(float factor, unsigned long thresholdMs = 80)
```

### Multiplexer Methods
```cpp
void setMuxControlPins(uint8_t pin1, uint8_t pin2, uint8_t pin3)
void setMuxChannel(uint8_t chan)
uint8_t getMuxChannel()
uint8_t getMuxControlPin(int pinNumb)
```

---

## Button/Switch Features

- **Debounce time:** 20ms default (`setDebounceTime()`)
- **Double-click window:** 350ms default (`setDoubleClickTime()`)
- **Double-click detection:** Uses press-to-press timing (more forgiving than release-to-press)
- **Hold threshold:** 500ms default (`setHoldTime()`) - one-shot event
- **Long-press threshold:** 1000ms default (`setLongPressTime()`) - continuous state
- **Hold action tracking:** `notifyHoldAction()` on external control movement

### Click Detection Methods
- `isPressed()` - Returns true while button is held down (continuous)
- `isDoubleClicked()` - Returns true once on second press of double-click
- `wasSingleClicked()` - Returns true once after release + double-click window expires
- `isClickPending()` - Returns true if waiting to confirm single vs double click

### Hold/Long-press Detection Methods
- `isHeld()` - Returns true once when hold threshold (500ms) reached (one-shot)
- `isLongPressed()` - Returns true continuously while held past long-press threshold (1000ms)
- `wasHeld()` - Returns true once on release if button was held

---

## Touch Sensing (ESP32 only)

### Overview

Capacitive touch sensing with multi-pad support. The touch system has three layers of protection against capacitive coupling noise (where touching one pad briefly disturbs readings on nearby pads).

### Protection Layers

1. **Baseline freeze while touched** — While a pad's `_touchState` is true, its baseline reference is frozen. Prevents coupling-induced value dips from corrupting the delta calculation. Baseline only adapts when untouched (gradual drift-up every ~200ms, and immediate sync on significant drops > 5 units).

2. **Minimum hold time** (default 30ms) — After touch-ON, OFF transitions are suppressed for `_touchMinHoldMs`. Coupling transients settle in 10-20ms, so 30ms catches them. Configure via `setTouchMinHold(ms)`. Don't set too high — it delays real releases and can cause voice accumulation in fast staccato playing.

3. **Debounce** (default 4 reads) — Requires N consecutive consistent readings before state changes. At 3ms polling, 4 reads = ~12ms. Filters brief coupling glitches. Configure via `setTouchDebounceReads(reads)`.

### Hysteresis

Separate ON/OFF thresholds prevent oscillation near the threshold:
- ON threshold: 22 (delta above baseline to trigger touch)
- OFF threshold: 16 (delta above baseline to release touch)
- Configure via `setTouchThresholds(on, off)`

### Retrigger Detection

Detects rapid lift-and-retouch while a pad remains in the touched state. Uses a dip-then-rise pattern — a per-read delta drop ≥ threshold followed by a per-read delta rise ≥ threshold. This avoids false triggers on initial press (no preceding dip), normal release (dip but no recovery), and hold noise (neither exceeds threshold).

- Check with `wasRetriggered()` after `isTouched()` (read-once flag)
- Configure sensitivity with `setRetriggerThreshold(threshold)` (default 15)
- **Disable for multi-pad use** with `setRetriggerThreshold(0)` — coupling noise creates false dip-rise patterns indistinguishable from real retriggers
- Retrigger detection is automatically suppressed during the minimum hold window
- `resetRetriggerState()` clears the retrigger state machine (pending dip/rise detection)

### Calibration

- `calibrateTouch(readings)` — Resets baseline and reads N samples over ~N×4ms to establish a stable reference. Call once at startup with pads untouched. Default 50 readings (~200ms).
- `resetTouchBaseline()` — Resets baseline to 65535 (will re-sync on next read). Use if touch behavior becomes erratic after environmental changes.

### Touch Configuration Summary

| Parameter | Default | Setter | Purpose |
|-----------|---------|--------|---------|
| `_touchOnThreshold` | 22 | `setTouchThresholds(on, off)` | Delta to trigger ON |
| `_touchOffThreshold` | 16 | `setTouchThresholds(on, off)` | Delta to trigger OFF |
| `_touchDebounceReads` | 4 | `setTouchDebounceReads(reads)` | Consecutive reads required |
| `_touchMinHoldMs` | 30 | `setTouchMinHold(ms)` | Min time before OFF allowed |
| `_retriggerThreshold` | 15 | `setRetriggerThreshold(threshold)` | Dip/rise sensitivity (0=disabled) |

### Multi-Pad Best Practices

When using multiple touch pads simultaneously:

1. **Calibrate at startup** with `calibrateTouch(50)` — pads must be untouched
2. **Set debounce to 4+** — lower values allow coupling-induced false state changes
3. **Disable retrigger** with `setRetriggerThreshold(0)` — coupling creates false retriggers
4. **Use default min hold (30ms)** — or increase if coupling is severe; keep under 50ms for responsiveness
5. **Track pad-to-voice mappings** at the application level — invalidate all stale mappings when stealing voices
6. **Check if pitch is already sounding** before re-triggering — prevents audible artifacts from false off/on cycles

See the `MultiControl_Touch_Pads` example for a complete implementation.

### Global Counters

`multiControlAnyButtonPressed`, `multiControlAnyTouchPressed`, `multiControlAnyPressed` — declared but not automatically updated by the library; available for user code to track.

---

## File Structure

```
/MultiControl/
├── MultiControl.h                          (single header, ~1470 lines)
├── MULTICONTROL_CODEBASE_ANALYSIS.md       (this file)
└── examples/
    ├── MultiControl_Test/                  (basic control test)
    ├── MultiControl_Bank_Test/             (bank switching demo)
    ├── MultiControl_Button_Gestures/       (click, double-click, hold, long-press)
    ├── MultiControl_Encoder/               (rotary encoder with acceleration + button)
    ├── MultiControl_Multiplex_Button_Test/ (multiplexed buttons)
    └── MultiControl_Touch_Pads/            (multi-pad touch with voice allocation)
```

---

## Usage Examples

### Basic Potentiometer
```cpp
MultiControl knob(15, 1);  // GPIO 15, potentiometer (1 = pot)
void loop() {
  int val = knob.readPot();
  if (val >= 0) {
    Serial.println(val);
  }
}
```

### With Bank Switching
```cpp
knob.initBanks(4);
knob.setBank(0);
int val = knob.readPot();  // Bank-aware reading
```

### Tuning Pot Smoothing
```cpp
knob.setSnapMultiplier(0.02);      // Slower response, smoother
knob.setActivityThreshold(8.0);    // More aggressive sleep
knob.setPotHysteresis(12);         // More hysteresis
```

### Multi-Pad Touch (ESP32)
```cpp
MultiControl pads[7];
int prevTouch[7];

void setup() {
  for (int i = 0; i < 7; i++) {
    pads[i].setPin(i + 1);
    pads[i].calibrateTouch(50);          // Auto-calibrate baseline
    pads[i].setTouchDebounceReads(4);    // Robust for multi-pad coupling
    pads[i].setRetriggerThreshold(0);    // Disable — unreliable with coupling
    prevTouch[i] = pads[i].isTouched();
  }
}

void loop() {
  for (int i = 0; i < 7; i++) {
    int val = pads[i].isTouched();
    if (val != prevTouch[i]) {
      prevTouch[i] = val;
      if (val) {
        Serial.print("Pad "); Serial.print(i); Serial.println(" ON");
      } else {
        Serial.print("Pad "); Serial.print(i); Serial.println(" OFF");
      }
    }
  }
  delay(3);
}
```

### Rotary Encoder with Acceleration
```cpp
MultiControl encoder;
encoder.setEncoderPins(15, 16, 17);  // A, B, push button
encoder.setEncoderRange(0, 100);
encoder.setEncoderPosition(50);
encoder.setEncoderAccel(5.0);        // up to 5x speed when turning fast
void loop() {
  int pos = encoder.readChanged();
  if (pos >= 0) Serial.println(pos);
  if (encoder.wasSingleClicked()) Serial.println("click");
  delay(1);
}
```

### Multiplexed Buttons
```cpp
MultiControl btn(16, 4); // 4 = mux button
btn.setMuxControlPins(12, 13, 14);  // CD4051 control pins
btn.setMuxChannel(5);
int state = btn.readMuxButton();
```
