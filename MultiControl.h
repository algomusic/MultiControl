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
      initBanks(_numBanks);
    };

    /** Constructor. 
    * @param pin The GPIO pin number to use for the control.
    */
    MultiControl(uint8_t pin): _pin(pin) {
      MultiControl(pin, 0);
    };

    /** Constructor. 
    * @param pin The GPIO pin number to use for the control.
    * @param controlType The type of control: 0 = touch, 1 = potentiometer, 2 = button, 3 = switch
    */
    MultiControl(uint8_t pin, uint8_t controlType): _pin(pin), _controlType(controlType) {
      setPin(pin);
      setControl(controlType);
      analogSetPinAttenuation(pin, ADC_11db); // ESP32
      initBanks(_numBanks);
    };

    /* Set the GPIO pin to use for this control
    * @param pin The GPIO pin number to use for the control.
    */
    void setPin(uint8_t pin) { 
      _pin = pin; 
      analogSetPinAttenuation(_pin, ADC_11db); // ESP32
    }

    uint8_t getPin() { return _pin; }

    /* Set the type of control.
    * @param controlType The type of control: 0 = touch, 1 = pot, 2 = button, 3 = switch
    */
    void setControl(uint8_t controlType) { 
      _controlType = controlType; 
      if (controlType == _SWITCH || controlType == _BUTTON) {
        pinMode(_pin, INPUT_PULLUP); // for buttons and switches
      }
      else if (controlType == _TOUCH || controlType == _POT) {
        pinMode(_pin, INPUT); // for touch or potentiometer
        digitalWrite(_pin, LOW); // disable internal pullup if set
      }
    }

    uint8_t getControl() { return _controlType; }

    /* Read the touch value */
    inline
    int readTouch() {
      if (_controlType != _TOUCH) {
        setControl(_TOUCH);
      }
      int readValue = touchRead(_pin)>>8;
      if (readValue < _minTouchValue) _minTouchValue = readValue;
      if (readValue > _maxTouchValue) _maxTouchValue = _minTouchValue * 4;
      int tVal = 0;
      if (readValue > _minTouchValue) {
        tVal = pow((readValue - _minTouchValue) / (float)(_maxTouchValue - _minTouchValue), 3) * 1023.0f;
      }
      tVal = min(1024, tVal);
      _prevTouchVal = (_prevTouchVal + tVal) * 0.5f;
      if (_prevTouchVal < 10) _prevTouchVal = 0;
      _touchValue = _prevTouchVal;
      bool prevTState = _touchState;
      _touchState = _prevTouchVal > 0;
      if (prevTState == true && _touchState == false) {
        multiControlAnyTouchPressed -= 1;
      } 
      if (prevTState == false && _touchState == true) {
         multiControlAnyTouchPressed += 1;
      }
      setValue(_touchValue);
      return _touchValue;
    }

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
      int val = digitalRead(_pin);
      if (val == 0 && _buttonValue == false) {
        _buttonValue = true;
        multiControlAnyButtonPressed += 1;
      }
      if (val == 1 && _buttonValue == true) {
        _buttonValue = false; 
        multiControlAnyButtonPressed -= 1;
      }
      setValue(val);
      return val;
    }

    bool isPressed() {
      readButton();
      bool returnVal = false;
      if (_buttonValue == 0) returnVal = true;
      return returnVal;
    }

    /* Read the potentiometer value 
    * @return The potentiometer value: 0 to 1023, or -1 if bank has been changed
    */
    inline
    int readPot() {
      if (_controlType != _POT) {
        setControl(_POT);
      }

      int readValue = analogRead(_pin) + analogRead(_pin);
      responsiveUpdate(readValue >> 4); // 3
      int retVal = responsiveValue * 2;
      
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
      return 0; // just in case
    }

    /* Return the read value if changed since, else return -1 */
    int changed() {
      int returnVal = -1;
      if (_controlType == 0) {
        int newVal = readTouch();
        if (newVal != _prevTouchVal) returnVal = newVal;
      }
      if (_controlType == 2) {
        int newVal = readButton();
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
    /* Setup the number of banks. 
    *  This will delete any existing banks.
    */
    void initBanks(int numBanks) {
      _numBanks = numBanks;
      delete[] _bankValues;
      _bankValues = new int[_numBanks];
      for (int i=0; i<_numBanks; i++) {
        _bankValues[i] = 0;
      }
      _bank = 0;
    }

    /* Choose the current bank */
    void setBank(uint8_t bank) { 
      _bank = bank % _numBanks; 
      _bankChanged = true;
      _latchAbove = false;
      _latchBelow = false;
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

    /* Set a particular bank's value */
    void setBankValue(int bank, int val) { 
      _bankValues[bank] = val;
    }

    /* Get the current bank's value */
    int getCurrentBankValue() { 
      return getValue(); 
    }

    /* Geta particular bank's value */
    int getBankValue(int bank) { 
      return _bankValues[bank]; 
    }

    /* Set the changed status of a bank */
    void setBankChanged(bool val) { 
      _bankChanged = val;
    }

  private:

    uint8_t _pin = 0;
    int _touchValue = 0; // 0 - 1023
    int8_t _touchState = false; // 0 or 1, true is touched
    int8_t _buttonValue = 0; // 0 or 1, true (1) is pressed
    int8_t _prevButtonValue = 0;
    int _minTouchValue = 1024; 
    int _maxTouchValue = 0; 
    int _prevTouchVal = 0;
    uint8_t _controlType = 0; // 0 = touch, 1 = pot, 2 = button, 3 = switch
    int _potValue = 0; // 0 - 1023
    int _prevPotRead = 0; // 0 - 1023
    unsigned long _readTime = 0;
    int * _potReadVals = new int[10];
    float _avePotReadVal = 0;
    uint8_t _potReadCnt = 0;
    int8_t _switchValue = 0; // 0 - 1
    const static uint8_t _TOUCH = 0;
    const static uint8_t _POT = 1;
    const static uint8_t _BUTTON = 2;
    const static uint8_t _SWITCH = 3;
    int _numBanks = 8;
    int * _bankValues = new int[_numBanks];
    uint8_t _bank = 0;
    bool _bankChanged = true;
    bool _latchBelow = false;
    bool _latchAbove = false;
    int _firstLatchValue = -1;
    bool _firstLatchChanged = false;
    // responsive read variables
    int analogResolution = 512; //256; //127; //1024;
    float snapMultiplier = 0.05; // 0.01
    bool sleepEnable = true;
    float activityThreshold = 4.0;
    bool edgeSnapEnable = true;
    float smoothValue;
    unsigned long lastActivityMS;
    float errorEMA = 0.0;
    bool sleeping = false;
    int rawValue;
    int responsiveValue;
    int prevResponsiveValue;
    bool responsiveValueHasChanged;

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
          if (val <= getCurrentBankValue()) {
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
      if(sleepEnable) {
        snap *= 0.5 + 0.5;
      }
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

};

#endif /* MULTICONTROL_H_ */