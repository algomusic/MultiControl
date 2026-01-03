/*
 * MultiControl.h
 *
 * Manage ESP32 GPIO pins for buttons, dials,  and switches.
 * by Andrew R. Brown 2023
 * 
 * Pot smoothing algorithms from the
 * ResponsiveAnalogRead library by Damien Clarke (2016)
 * https://github.com/dxinteractive/ResponsiveAnalogRead
 * 
 * Designed for use with the ESP32 microcontroller, the OnBoard sProject PCB, and the Arduino IDE.
 * Assumes value ranges for 0 to 1023 for continuous control values (dials, cap touch) and 0 or 1 for on/off states (buttons, switches).
 * 
 * MultiControl is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 */

#ifndef MULTICONTROL_H_
#define MULTICONTROL_H_

int multiControlAnyTouchPressed = 0;
int multiControlAnyButtonPressed = 0;
int multiControlAnyPressed = 0;
float MAX_10_INV = 0.0009765625f;

class MultiControl {
  public:
    /** Constructor. */
    MultiControl() {
      initBanks(1);  // Start with 1 bank to save memory
    };

    /** Destructor - free dynamic memory */
    ~MultiControl() {
      if (_bankValues != nullptr) {
        delete[] _bankValues;
        _bankValues = nullptr;
      }
    };

    /** Constructor. 
    * @param pin The GPIO pin number to use for the control.
    */
    MultiControl(uint8_t pin): _pin(pin) {
      MultiControl(pin, 0);
    };

    /** Constructor.
    * @param pin The GPIO pin number to use for the control.
    * @param controlType The type of control: 0 = touch, 1 = potentiometer, 2 = button, 3 = switch, 4 = muxButton
    * Note that muxButton requires also setting the muxControlPins and muxChannel
    */
    MultiControl(uint8_t pin, uint8_t controlType): _pin(pin), _controlType(controlType) {
      setPin(pin);
      setControl(controlType);
      // analogSetPinAttenuation(pin, ADC_11db); // ESP32
      initBanks(1);  // Start with 1 bank to save memory
    };

    /* Set the GPIO pin to use
    * @param pin1 The GPIO pin number to use for the control.
    */
    void setPin(uint8_t pin) { 
      _pin = pin; 
      if (_controlType == _MUX_BUTTON) pinMode(_pin, INPUT_PULLUP);
      // analogSetPinAttenuation(_pin, ADC_11db); // ESP32
    }

    /* Return the GPIO pin in use */
    uint8_t getPin() { return _pin; }

    /* Set the GPIO pins to use to control the multiplex channel
    * Tested with CD4051
    * @param pin1 The GPIO pin number to use for the LSB.
    * @param pin2 The GPIO pin number to use for the middle bit.
    * @param pin3 The GPIO pin number to use for the MSB.
    */
    void setMuxControlPins(uint8_t pin1, uint8_t pin2, uint8_t pin3) {
      if (_controlType != _MUX_BUTTON) {
        setControl(_MUX_BUTTON);
      }
      _muxControlPins[0] = pin1;
      _muxControlPins[1] = pin2;
      _muxControlPins[2] = pin3;
      // Setup MUX control pins
      for (int i = 0; i < 3; i++) {
        pinMode(_muxControlPins[i], OUTPUT);
      }
    }

    /* Retrieve one of the GPIO pins used to control the multiplex channel 
    * @pinNumb The mux control pin (0, 1 or 2) to get.
    */
    uint8_t getMuxControlPin(int pinNumb) { 
      if (pinNumb == 0) return _muxControlPins[0];
      if (pinNumb == 1) return _muxControlPins[1];
      if (pinNumb == 2) return _muxControlPins[2];
      return -1;
    }

    /* Set the multiplex channel
    * @param chan The mux channel.
    */
    void setMuxChannel(uint8_t chan) {
      if (_controlType != _MUX_BUTTON) {
        setControl(_MUX_BUTTON);
      }
      _muxChannel = chan; 
    }

     /* Retrieve the multiplex channel in use */
    uint8_t getMuxChannel() { 
      return _muxChannel; 
    }

    /* Set the type of control.
    * @param controlType The type of control: 0 = touch, 1 = pot, 2 = button, 3 = switch, 4 = muxButton
    */
    void setControl(uint8_t controlType) {
      _controlType = controlType;
      if (controlType == _SWITCH || controlType == _BUTTON || controlType == _MUX_BUTTON) {
        pinMode(_pin, INPUT_PULLUP); // for buttons and switches
        // Initialize button debounce state to prevent false triggers on startup
        _lastButtonChangeTime = millis();
        _rawButtonState = digitalRead(_pin);
        _debouncedButtonState = _rawButtonState;
        _buttonValue = (_rawButtonState == 0);  // true if pressed
      } else if (controlType == _POT || controlType == _TOUCH) {
        pinMode(_pin, INPUT); // for touch or potentiometer
        digitalWrite(_pin, LOW); // disable internal pullup if set
      }
    }

      /* Retrieve the type of control in use */
    uint8_t getControl() { return _controlType; }

    /* Read the touch value (ESP32 only - returns 0 on unsupported platforms) */
    inline
    int readTouch() {
      #if defined(ESP32)
        if (_controlType != _TOUCH) {
          setControl(_TOUCH);
        }
        _touchValue = touchRead(_pin) >> 8;

        // Update baseline - but ignore small drops that could be capacitive coupling
        // from other touched pads. Only sync if significantly lower (initial calibration)
        // or if baseline is uninitialized.
        if (_touchBaseline == 65535) {
          // First read after reset - sync immediately
          _touchBaseline = _touchValue;
        } else if (_touchValue < _touchBaseline - 5) {
          // Significant drop (> 5 units) - likely real environmental change, sync
          _touchBaseline = _touchValue;
        }
        // Small drops (1-5 units) are ignored - likely capacitive coupling from other touches

        // Adaptive baseline: slowly drift up when not touched to handle environmental changes
        if (!_touchState) {
          _baselineDriftCounter++;
          if (_baselineDriftCounter >= 50 && _touchValue > _touchBaseline) {  // ~200ms at 4ms polling
            _touchBaseline++;
            _baselineDriftCounter = 0;
          }
        } else {
          _baselineDriftCounter = 0;  // Reset counter while touched
        }

        int delta = _touchValue - _touchBaseline;
        bool newState = _touchState;  // Start with current state

        // Hysteresis: use different thresholds for on vs off
        if (_touchState) {
          // Currently touched - use lower threshold to release (prevents stuck notes)
          if (delta < _touchOffThreshold) {
            newState = false;
          }
        } else {
          // Currently not touched - use higher threshold to engage (prevents false triggers)
          if (delta > _touchOnThreshold) {
            newState = true;
          }
        }

        // Debouncing: require consecutive consistent readings before changing state
        if (newState != _touchState) {
          _touchDebounceCount++;
          if (_touchDebounceCount >= _touchDebounceReads) {
            _touchState = newState;
            _touchDebounceCount = 0;
          }
        } else {
          _touchDebounceCount = 0;  // Reset counter when state matches
        }

        _touchValue = min(1024, max(0, delta) * 10);  // Scale to 0-1024
        setValue(_touchValue);
        return _touchValue;
      #else
        // Touch not supported on this platform
        _touchValue = 0;
        _touchState = false;
        return 0;
      #endif
    }

    /* Check if the control is touched or not (ESP32 only - returns false on unsupported platforms) */
    inline
    bool isTouched() {
      readTouch();
      return _touchState;
    }

    void setButtonValue(bool value) { _buttonValue = value; } // 0 or false is off, 1 or true is on

    /* Read the button value
    * @return The button value: 0 or false is off, 1 or true is on
    */
    inline
    int readButton() {
      if (_controlType != _BUTTON) {
        setControl(_BUTTON);
      }
      int rawVal = digitalRead(_pin);
      unsigned long now = millis();

      // Debouncing: track raw state changes
      if (rawVal != _rawButtonState) {
        _lastButtonChangeTime = now;
        _rawButtonState = rawVal;
      }

      // Only update debounced state if stable for debounce period
      int val = _debouncedButtonState;
      if ((now - _lastButtonChangeTime) >= _debounceTime) {
        if (_rawButtonState != _debouncedButtonState) {
          _debouncedButtonState = _rawButtonState;
          val = _debouncedButtonState;
        }
      }

      if (val == 0 && _buttonValue == false) {
        _buttonValue = true;
        multiControlAnyButtonPressed += 1;
        // Record press time for hold detection
        _pressStartTime = now;
        _holdTriggered = false;
        _held = false;
        _wasHeldOnRelease = false;
        _holdActionOccurred = false;
        _hadHoldAction = false;
        // Check for double-click on press
        if (_pressStartTime - _lastReleaseTime < _doubleClickTime) {
          _doubleClicked = true;
        }
      }
      if (val == 1 && _buttonValue == true) {
        _buttonValue = false;
        multiControlAnyButtonPressed -= 1;
        // Record release time for double-click detection
        _lastReleaseTime = now;
        // Save hold and action state before reset (for release checks)
        _wasHeldOnRelease = _holdTriggered;
        _hadHoldAction = _holdActionOccurred;
        // Reset hold state on release
        _holdTriggered = false;
        _held = false;
        _holdActionOccurred = false;
      }
      // Check for hold while pressed
      if (_buttonValue && !_holdTriggered && (now - _pressStartTime >= _holdTime)) {
        _held = true;
        _holdTriggered = true;
      }
      setValue(val);
      return val;
    }

    bool isPressed() {
      uint8_t val = 1;
      if (_controlType == 2) val = readButton();
      if (_controlType == 4) val = readMuxButton();
      bool returnVal = false;
      if (val == 0) returnVal = true;
      return returnVal;
    }

    /** Check if the button was double-clicked
     * Call this after isPressed() in your button handling code.
     * Returns true only once per double-click event (on the second press).
     * @return true if double-click detected, false otherwise
     */
    bool isDoubleClicked() {
      bool result = _doubleClicked;
      _doubleClicked = false;  // Clear after reading
      return result;
    }

    /** Set the double-click detection time window
     * @param ms Time window in milliseconds (default 300)
     */
    void setDoubleClickTime(unsigned long ms) {
      _doubleClickTime = ms;
    }

    /** Get the current double-click time window */
    unsigned long getDoubleClickTime() {
      return _doubleClickTime;
    }

    /** Check if the button is being held
     * Returns true only once per hold event (when hold time threshold is reached).
     * @return true if hold detected, false otherwise
     */
    bool isHeld() {
      bool result = _held;
      _held = false;  // Clear after reading
      return result;
    }

    /** Set the hold detection time threshold
     * @param ms Time in milliseconds to trigger hold (default 500)
     */
    void setHoldTime(unsigned long ms) {
      _holdTime = ms;
    }

    /** Get the current hold time threshold */
    unsigned long getHoldTime() {
      return _holdTime;
    }

    /** Set the debounce time for button readings
     * @param ms Time in milliseconds to debounce (default 20)
     */
    void setDebounceTime(unsigned long ms) {
      _debounceTime = ms;
    }

    /** Get the current debounce time */
    unsigned long getDebounceTime() {
      return _debounceTime;
    }

    /** Set touch detection thresholds for hysteresis
     * @param onThreshold Value above baseline to trigger touch ON (default 20)
     * @param offThreshold Value above baseline to trigger touch OFF (default 8)
     * The gap between these values prevents oscillation near the threshold.
     */
    void setTouchThresholds(int16_t onThreshold, int16_t offThreshold) {
      _touchOnThreshold = onThreshold;
      _touchOffThreshold = offThreshold;
    }

    /** Get touch ON threshold */
    int16_t getTouchOnThreshold() { return _touchOnThreshold; }

    /** Get touch OFF threshold */
    int16_t getTouchOffThreshold() { return _touchOffThreshold; }

    /** Set pot hysteresis (minimum change required to report new value)
     * Higher values reduce jitter but decrease sensitivity
     * @param hysteresis Threshold value (default 3, try 8-15 for less jitter)
     */
    void setPotHysteresis(int hysteresis) {
      _potHysteresis = max(1, hysteresis);
    }

    /** Get pot hysteresis threshold */
    int getPotHysteresis() { return _potHysteresis; }

    /** Set touch debounce read count
     * @param reads Number of consecutive consistent readings required (default 4)
     * At 4ms polling interval, 4 reads = ~16ms debounce
     */
    void setTouchDebounceReads(uint8_t reads) {
      _touchDebounceReads = reads;
    }

    /** Get touch debounce read count */
    uint8_t getTouchDebounceReads() { return _touchDebounceReads; }

    /** Reset touch baseline to allow recalibration
     * Call this if touch behavior becomes erratic after environmental changes
     */
    void resetTouchBaseline() {
      _touchBaseline = 65535;
      _baselineDriftCounter = 0;
      _touchDebounceCount = 0;
    }

    /** Calibrate touch baseline at startup
     * Call this in setup() after setControl(0) to prevent false triggers.
     * Performs multiple readings over ~200ms to stabilize the baseline.
     * @param readings Number of calibration readings (default 50, ~200ms at 4ms intervals)
     */
    void calibrateTouch(int readings = 50) {
      resetTouchBaseline();
      for (int i = 0; i < readings; i++) {
        readTouch();  // Each read updates the baseline
        delay(4);
      }
      // Clear any false touch state from calibration period
      _touchState = false;
      _touchDebounceCount = 0;
    }

    /** Check if button was held (for use on release - returns state from before reset) */
    bool wasHeld() {
      return _wasHeldOnRelease;
    }

    /** Notify that an external action occurred while button is pressed/held
     * Call this when another control is moved while this button is pressed.
     * Use isHeldAndActioned() to check, or hadHoldAction() on release.
     */
    void notifyHoldAction() {
      if (_buttonValue) {  // Track from press, not just hold
        _holdActionOccurred = true;
      }
    }

    /** Check if button is currently held AND an action has occurred
     * @return true if held and action notified, false otherwise
     */
    bool isHeldAndActioned() {
      return _holdTriggered && _holdActionOccurred;
    }

    /** Check if action occurred during hold (for use on release)
     * @return true if hold had an associated action, false otherwise
     */
    bool hadHoldAction() {
      return _hadHoldAction;
    }

    /* Read the mutiplexed button value
    * @return The button value: 0 or false is off, 1 or true is on
    */
    inline int readMuxButton() {
      if (_controlType != _MUX_BUTTON) {
        setControl(_MUX_BUTTON);
      }
      muxWrite();
      delayMicroseconds(10); // Allow MUX to settle
      int rawVal = digitalRead(_pin);
      unsigned long now = millis();

      // Debouncing: track raw state changes
      if (rawVal != _rawButtonState) {
        _lastButtonChangeTime = now;
        _rawButtonState = rawVal;
      }

      // Only update debounced state if stable for debounce period
      int val = _debouncedButtonState;
      if ((now - _lastButtonChangeTime) >= _debounceTime) {
        if (_rawButtonState != _debouncedButtonState) {
          _debouncedButtonState = _rawButtonState;
          val = _debouncedButtonState;
        }
      }

      if (val == 0 && _buttonValue == false) {
        _buttonValue = true;
        multiControlAnyButtonPressed += 1;
        // Record press time for hold detection
        _pressStartTime = now;
        _holdTriggered = false;
        _held = false;
        _wasHeldOnRelease = false;
        _holdActionOccurred = false;
        _hadHoldAction = false;
        // Check for double-click on press
        if (_pressStartTime - _lastReleaseTime < _doubleClickTime) {
          _doubleClicked = true;
        }
      }
      if (val == 1 && _buttonValue == true) {
        _buttonValue = false;
        multiControlAnyButtonPressed -= 1;
        // Record release time for double-click detection
        _lastReleaseTime = now;
        // Save hold and action state before reset (for release checks)
        _wasHeldOnRelease = _holdTriggered;
        _hadHoldAction = _holdActionOccurred;
        // Reset hold state on release
        _holdTriggered = false;
        _held = false;
        _holdActionOccurred = false;
      }
      // Check for hold while pressed
      if (_buttonValue && !_holdTriggered && (now - _pressStartTime >= _holdTime)) {
        _held = true;
        _holdTriggered = true;
      }
      setValue(val);
      return val;
    }

    /* Read the potentiometer value
    * @return The potentiometer value: 0 to 1023, or -1 if bank has been changed
    */
    inline
    int readPot() {
      if (_controlType != _POT) {
        setControl(_POT);
      }

      // Take 4 samples with settling time
      int samples[4];
      for (int s = 0; s < 4; s++) {
        samples[s] = analogRead(_pin);
        if (s < 3) delayMicroseconds(10);
      }
      // Simple sort for median of middle two (rejects outliers)
      for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 4; j++) {
          if (samples[j] < samples[i]) {
            int tmp = samples[i];
            samples[i] = samples[j];
            samples[j] = tmp;
          }
        }
      }

      // Detect floating/disconnected pin - samples too erratic
      int sampleSpread = samples[3] - samples[0];  // max - min (sorted)
      if (sampleSpread > _maxSampleSpread) {
        return -3;  // unstable reading, likely floating pin
      }

      // Sticky edges - lock to 0 or 1023 when all samples are near extremes
      if (samples[3] < 30) {  // all samples below 30 (sorted, so [3] is max)
        responsiveValue = 0;
        smoothValue = 0;
        int retVal = 0;
        retVal = checkBank(retVal);
        if (retVal >= 0) setValue(retVal);
        return retVal;
      }
      if (samples[0] > 4065) {  // all samples above 4065 (sorted, so [0] is min)
        responsiveValue = 511;
        smoothValue = 511;
        int retVal = 1022;
        retVal = checkBank(retVal);
        if (retVal >= 0) setValue(retVal);
        return retVal;
      }

      int readValue = samples[1] + samples[2];  // middle two values
      responsiveUpdate(readValue >> 4);
      int retVal = responsiveValue * 2;

      // Output hysteresis - suppress small fluctuations except near edges
      if (abs(retVal - _potValue) < _potHysteresis && retVal > 2 && retVal < 1020) {
        retVal = _potValue;
      }

      // slew reading to smooth out rapid changes and increase reported resolution
      float slewVal = slew((float)_potValue, (float)retVal, 0.5f);
      retVal = (int)(slewVal + 0.5f); 

      if (readValue == 0) {
        retVal = min(checkBank(readValue), retVal);
      } else retVal = checkBank(retVal);
      if (retVal >= 0) setValue(retVal);
      return retVal;
    }

    /* Read the switch value */
    inline
    int readSwitch() {
      if (_controlType != _SWITCH) {
        setControl(_SWITCH);
      }
      int val = digitalRead(_pin);
      val = checkBank(val);
      if (val >= 0) setValue(val);
      return val;
    }

    /* Is switch in the On position? */
    bool isSwitchedOn() {
      readSwitch();
      return _switchValue;
    }

    /* Read the control based on the set type */
    int read() {
      if (_controlType == 0) return readTouch();
      if (_controlType == 1) return readPot();
      if (_controlType == 2) return readButton();
      if (_controlType == 3) return readSwitch();
      if (_controlType == 4) return readMuxButton();
      return 0; // just in case
    }

    /* Return the read value if changed, otherwise return -1 */
    int readChanged() {
      // Serial.println("readChanged");
      int returnVal = -1;
      if (_controlType == 0) {
        int prevVal = _prevTouchValue;
        int newVal = readTouch();
        if (newVal != prevVal) returnVal = newVal;
      }
      if (_controlType == 1) {
        int prevVal = _potValue;
        int newVal = readPot();
        if (newVal != prevVal) returnVal = newVal;
      }
      if (_controlType == 2) {
        int newVal = readButton();
        if (newVal != _prevButtonValue) {
          returnVal = newVal;
          _prevButtonValue = newVal;
        }
      }
      if (_controlType == 3) {
        int8_t prevVal = _switchValue;
        int8_t newVal = readSwitch();
        if (newVal != prevVal) returnVal = newVal;
      }
      if (_controlType == 4) {
        int newVal = readMuxButton();
        if (newVal != _prevButtonValue) {
          returnVal = newVal;
          _prevButtonValue = newVal;
        }
      }
      return returnVal;
    }

    /* Return the control value on the controller type */
    int getValue() {
      return _bankValues[_bank]; 
    }

    /* Sepcify the control value on the controller type */
    void setValue(int type, int val) {
      val = max(0, min(1024, val));
      _bankValues[_bank] = val;
      _controlType = type;
      if (_controlType == 0) _touchValue = val;
      if (_controlType == 1) _potValue = val;
      if (_controlType == 2) _buttonValue = val;
      if (_controlType == 3) _switchValue = val;
    }

    /* Sepcify the control value on the controller type */
    void setValue(int val) {
      setValue(_controlType, val);
    }

    // banks
    /* Ensure bank array has capacity for the required number of banks.
    *  Dynamically grows the array if needed, preserving existing values.
    */
    void ensureBankCapacity(int requiredBanks) {
      if (requiredBanks <= _numBanks) return;

      // Allocate new array
      int* newValues = new int[requiredBanks];

      // Copy existing values
      for (int i = 0; i < _numBanks; i++) {
        newValues[i] = _bankValues[i];
      }
      // Initialize new slots to 0
      for (int i = _numBanks; i < requiredBanks; i++) {
        newValues[i] = 0;
      }

      // Free old array and use new one
      if (_bankValues != nullptr) {
        delete[] _bankValues;
      }
      _bankValues = newValues;
      _numBanks = requiredBanks;
    }

    /* Setup the number of banks.
    *  Grows array if needed, resets all bank values to 0.
    */
    void initBanks(int numBanks) {
      ensureBankCapacity(numBanks);
      for (int i = 0; i < _numBanks; i++) {
        _bankValues[i] = 0;
      }
      _bank = 0;
    }

    /* Choose the current bank - grows array if needed */
    void setBank(uint8_t bank) {
      if (bank >= _numBanks) {
        ensureBankCapacity(bank + 1);
      }
      _bank = bank;
      _bankChanged = true;
      _latchAbove = false;
      _latchBelow = false;
      // Sync _potValue with new bank's value to ensure hysteresis works correctly
      _potValue = _bankValues[_bank];
      // Serial.println("Bank changed. Current bank: " + String(_bank) + " Value: " + String(getValue()));
    }

    /* Get the current bank */
    int getBank() {
      return _bank;
    }

    /* Set the current bank's value */
    void setCurrentBankValue(int val) {
      setBankValue(_bank, val);
    }

    /* Set a particular bank's value - grows array if needed */
    void setBankValue(int bank, int val) {
      if (bank >= _numBanks) {
        ensureBankCapacity(bank + 1);
      }
      _bankValues[bank] = val;
    }

    /* Get the current bank's value */
    int getCurrentBankValue() {
      return getValue();
    }

    /* Get a particular bank's value (returns 0 if bank not yet allocated) */
    int getBankValue(int bank) {
      if (bank >= _numBanks) return 0;
      return _bankValues[bank];
    }

    /* Set the changed status of a bank */
    void setBankChanged(bool val) {
      _bankChanged = val;
    }

    // Latching API

    /** Enable or disable latching when changing banks.
     * When enabled (default), pot values are ignored after bank change until
     * the physical pot position crosses the stored bank value.
     * When disabled, pot values update immediately on bank change.
     * @param enabled true to enable latching, false to disable
     */
    void setLatchEnabled(bool enabled) {
      _latchEnabled = enabled;
      if (!enabled) {
        releaseLatch();
      }
    }

    /** Check if latching is enabled */
    bool isLatchEnabled() {
      return _latchEnabled;
    }

    /** Manually release the current latch.
     * Allows pot values to update immediately without needing to
     * cross the stored bank value threshold.
     */
    void releaseLatch() {
      _bankChanged = false;
      _latchAbove = true;
      _latchBelow = true;
      _firstLatchValue = -1;
      _firstLatchChanged = false;
    }

    /** Check if currently latched (waiting for pot to cross threshold).
     * @return true if latched and ignoring pot movements, false if updating normally
     */
    bool isLatched() {
      return _bankChanged && !(_latchAbove && _latchBelow && _firstLatchChanged);
    }

  private:

    uint8_t _pin = 0;
    int _touchValue = 0; // 0 - 1023
    int8_t _touchState = false; // 0 or 1, true is touched
    int8_t _buttonValue = 0; // 0 or 1, true (1) is pressed
    int8_t _prevButtonValue = 0;
    // Double-click detection
    unsigned long _lastReleaseTime = 0;  // timestamp of last button release
    unsigned long _doubleClickTime = 300;  // ms window for double-click detection
    bool _doubleClicked = false;  // flag set when double-click detected
    // Hold detection
    unsigned long _pressStartTime = 0;  // timestamp when button was pressed
    unsigned long _holdTime = 500;  // ms to trigger hold
    bool _held = false;  // flag set when hold detected
    bool _holdTriggered = false;  // ensures hold only triggers once per press
    bool _wasHeldOnRelease = false;  // preserved hold state for release detection
    bool _holdActionOccurred = false;  // external action during hold
    bool _hadHoldAction = false;  // preserved action state for release detection
    // Button debouncing
    unsigned long _debounceTime = 20;  // ms debounce window
    unsigned long _lastButtonChangeTime = 0;  // when raw state last changed
    int8_t _rawButtonState = 1;  // raw (undebounced) button reading
    int8_t _debouncedButtonState = 1;  // stable debounced state (1 = released)
    int _minTouchValue = 1024; 
    int _maxTouchValue = 0; 
    int _prevTouchValue = 0;
    uint8_t _controlType = 0; // 0 = touch, 1 = pot, 2 = button, 3 = switch, 4 = muxButton
    int _potValue = 0; // 0 - 1023
    int _potHysteresis = 3; // Minimum change required to report new value (default 3, increase for less jitter)
    int8_t _switchValue = 0; // 0 - 1
    const static uint8_t _TOUCH = 0;
    const static uint8_t _POT = 1;
    const static uint8_t _BUTTON = 2;
    const static uint8_t _SWITCH = 3;
    const static uint8_t _MUX_BUTTON = 4;
    int _numBanks = 0;
    int* _bankValues = nullptr;  // Dynamic allocation - grows as needed
    uint8_t _bank = 0;
    bool _bankChanged = true;
    bool _latchEnabled = true;  // Enable/disable latching on bank change
    bool _latchBelow = false;
    bool _latchAbove = false;
    int _firstLatchValue = -1;
    bool _firstLatchChanged = false;
    // responsive read variables
    int analogResolution = 512; // 0-511 input range from readPot() >>4 shift
    float snapMultiplier = 0.05; // 0.01
    bool sleepEnable = true;
    float activityThreshold = 4.0;
    bool edgeSnapEnable = true;
    int _maxSampleSpread = 50;  // max allowed spread between min/max samples (detects floating pins)
    float smoothValue = 0.0;
    unsigned long lastActivityMS = 0;
    float errorEMA = 0.0;
    bool sleeping = false;
    int rawValue = 0;
    int responsiveValue = 0;
    int prevResponsiveValue = 0;
    bool responsiveValueHasChanged = false;
    bool _firstRead = true;
    uint8_t _muxControlPins[3] = {0};  // Static allocation (was dynamic new uint8_t[])
    uint8_t _muxChannel = 0;
    uint16_t _touchBaseline = 65535;  // Start high, will be reduced by actual readings
    // Touch hysteresis and debouncing
    int16_t _touchOnThreshold = 22;    // Higher threshold to turn ON (prevents false triggers)
    int16_t _touchOffThreshold = 16;   // Threshold to turn OFF - raised from 8 to handle capacitive coupling when multiple pads touched
    uint8_t _touchDebounceCount = 0;   // Counter for consecutive consistent readings
    uint8_t _touchDebounceReads = 4;   // Required consecutive reads (4 reads × 4ms = ~16ms debounce)
    uint16_t _baselineDriftCounter = 0;      // Counter for slow baseline adaptation

    /** Return a partial increment toward target from current value
    * @curr The curent value
    * @target The desired final value
    * @amt The percentage toward target (0.0 - 1.0)
    */
    inline
    float slew(float curr, float target, float amt) {
      float dist = target - curr;
      return curr + dist * amt;
    }

    /* Check if the bank has changed and if so, set the pot and switch to latch
    * so as not to update until the value passes the previous value of that bank.
    * Return -1 or -2 when the bank has changed and the value should not be updated.
    * -1 means the value is below the bank value, -2 means the value is above the bank value.
    */
    int checkBank(int val) {
      // Skip latching logic if disabled
      if (!_latchEnabled) {
        _bankChanged = false;
        return min(1023, val);
      }

      if (_bankChanged) {
        if(_firstLatchValue == -1) _firstLatchValue = val;
        if (val != _firstLatchValue) {
          _firstLatchChanged = true;
        }
        if (!_latchAbove) {
          if (val >= getCurrentBankValue() || (val > 1000 && getCurrentBankValue() > 1000)) {
            _latchAbove = true;
          }
        }
        if (!_latchBelow) {
          if (val <= getCurrentBankValue() || (getCurrentBankValue() == 0 && val < 10)) {
            _latchBelow = true;
          }
        }
        if (_latchAbove && _latchBelow && _firstLatchChanged) {
          _bankChanged = false;
          _firstLatchValue = -1; // reset
          _firstLatchChanged = false;
        } else { // don't return anything until the bank has changed
          if (val < getCurrentBankValue()) {
            val = -1; // below val
          } else val = -2; // above val
        }
      }
      return min(1023, val);
    }

    /* responive read functions */
    void responsiveUpdate(int rawValueRead) {
      rawValue = rawValueRead;
      if (_firstRead) {
        smoothValue = rawValue;  // sync to actual pot position on first read
        _firstRead = false;
      }
      prevResponsiveValue = responsiveValue;
      responsiveValue = getResponsiveValue(rawValue);
      responsiveValueHasChanged = responsiveValue != prevResponsiveValue;
    }

    int getResponsiveValue(int newValue) {
      if(sleepEnable && edgeSnapEnable) {
        if(newValue < activityThreshold) {
          newValue = (newValue * 2) - activityThreshold;
        } else if(newValue > analogResolution - activityThreshold) {
          newValue = (newValue * 2) - analogResolution + activityThreshold;
        }
        if(newValue < 0) newValue = 0;  // prevent negative values from edge snap
      }
      unsigned int diff = abs(newValue - smoothValue);
      errorEMA += ((newValue - smoothValue) - errorEMA) * 0.4;
      if(sleepEnable) {
        sleeping = abs(errorEMA) < activityThreshold;
      }
      if(sleepEnable && sleeping) {
        return (int)smoothValue;
      }
      float snap = snapCurve(diff * snapMultiplier);
      smoothValue += (newValue - smoothValue) * snap;
      if(smoothValue < 0.0) {
        smoothValue = 0.0;
      } else if(smoothValue > analogResolution - 1) {
        smoothValue = analogResolution - 1;
      }
      return (int)smoothValue;
    }

    float snapCurve(float x) {
      float y = 1.0 / (x + 1.0);
      y = (1.0 - y) * 2.0;
      if(y > 1.0) {
        return 1.0;
      }
      return y;
    }

    void setSnapMultiplier(float newMultiplier) {
      if(newMultiplier > 1.0) {
        newMultiplier = 1.0;
      }
      if(newMultiplier < 0.0) {
        newMultiplier = 0.0;
      }
      snapMultiplier = newMultiplier;
    }

    /** Set max allowed sample spread for floating pin detection
     * @param spread Max difference between min/max of 4 samples (default 150)
     *               Set to 4096 to disable floating pin detection
     */
    void setMaxSampleSpread(int spread) {
      _maxSampleSpread = spread;
    }

    // Set to MUX control pins for the current channel
    void muxWrite() {
      for (int i = 0; i < 3; i++) {
        int pinState = bitRead(_muxChannel, i);
        digitalWrite(_muxControlPins[i], pinState);
      }
    }

};

#endif /* MULTICONTROL_H_ */