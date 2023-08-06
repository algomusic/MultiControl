/*
 * MultiControl.h
 *
 * Manage ESP32 GPIO pins for buttons, dials,  and switches.
 * by Andrew R. Brown 2023
 * 
 * Designed for use with the ESP32 microcontroller, the OnBoard sProject PCB, and the Arduino IDE.
 * Assumes value ranges for 0 to 1023 for continuous control values (dials, ) and 0 or 1 for on/off states (buttons, switches).
 * 
 * MultiControl is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 */

#ifndef MULTICONTROL_H_
#define MULTICONTROL_H_

int multiControlAnyTouchPressed = 0;
int multiControlAnyButtonPressed = 0;
int multiControlAnyPressed = 0;

class MultiControl {
  public:
    /** Constructor. */
    MultiControl() {};

    /** Constructor. 
    * @param pin The GPIO pin number to use for the control.
    */
    MultiControl(uint8_t pin): _pin(pin) {
      pinMode(pin, INPUT); // default to Touch
      analogSetPinAttenuation(pin, ADC_11db); // ESP32
    };

    /** Constructor. 
    * @param pin The GPIO pin number to use for the control.
    * @param controlType The type of control: 0 = , 1 = potentiometer, 2 = button, 3 = switch
    */
    MultiControl(uint8_t pin, uint8_t controlType): _pin(pin), _controlType(controlType) {
      setPin(pin);
      setControl(controlType);
      analogSetPinAttenuation(pin, ADC_11db); // ESP32
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
      int readVal = touchRead(_pin)>>8;
      if (readVal < _minTouchVal) _minTouchVal = readVal;
      if (readVal > _maxTouchVal) _maxTouchVal = _minTouchVal * 5;
      int tVal = 0;
      if (readVal > _minTouchVal) {
        tVal = (readVal - _minTouchVal) / (float)(_maxTouchVal - _minTouchVal) * 1024.0f;
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
      return _touchValue;
    }

    inline
    bool isTouched() {
      readTouch();
      return _touchState;
    }

    void setbuttonValue(bool value) { _buttonValue = value; } // 0 or false is off, 1 or true is on

    void setButtonValue(bool value) { _buttonValue = value; } // 0 or false is off, 1 or true is on

    /* Read the button value 
    * @return The button value: 0 or false is off, 1 or true is on
    */
    inline
    int readButton() {
      if (_controlType != _BUTTON) {
        setControl(_BUTTON);
      }
      int buttonValue = digitalRead(_pin);
      if (buttonValue == 0 && _buttonValue == false) {
        _buttonValue = true;
        multiControlAnyButtonPressed += 1;
      }
      if (buttonValue == 1 && _buttonValue == true) {
        _buttonValue = false; 
        multiControlAnyButtonPressed -= 1;
      }
      return _buttonValue;
    }

    bool isPressed() {
      readButton();
      return _buttonValue;
    }

    /* Read the potentiometer value 
    * @return The potentiometer value: 0 to 1023, or -1 if bank has been changed
    */
    inline
    int readPot() {
      if (_controlType != _POT) {
        setControl(_POT);
      }
      int tempPotVal = _potValue;
      int readVal = analogRead(_pin) >> 2;
      // smooth by averaging the last 10 readings
      _potReadVals[_potReadCnt] = readVal;
      _potReadCnt++;
      if (_potReadCnt >= 10) {
        _potReadCnt = 0;
      }
      int minVal = 10000;
      int maxVal = 0;
      int minIndex = 0;
      int maxIndex = 1;
      for (int i = 0; i < 10; i++) {
        int val = _potReadVals[i];
        if (val < minVal) {
          minVal = val;
          minIndex = i;
        }
        if (val > maxVal) {
          maxVal = val;
          maxIndex = i;
        }
      }
      _avePotReadVal = 0;
      for (int i = 0; i < 10; i++) {
        if (i != minIndex || i != maxIndex) _avePotReadVal += _potReadVals[i];
      }
      _avePotReadVal = _avePotReadVal >> 3;
      _potValue = slew(_avePotReadVal, _potValue, 0.2f);
      int retVal = _potValue;
      retVal = checkBank(retVal);
      return retVal;
    }

    /* Read the switch value */
    inline
    int readSwitch() {
      if (_controlType != _SWITCH) {
        setControl(_SWITCH);
      }
      _switchValue = digitalRead(_pin);
      _switchValue = checkBank(_switchValue);
      return _switchValue;
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

    /* Return the control value on the controller type */
    int getValue() {
      if (_controlType == 0) return _touchValue;
      if (_controlType == 1) return _potValue;
      if (_controlType == 2) return _buttonValue;
      if (_controlType == 3) return _switchValue;
      return 0; // just in case
    }

    // banks
    /* Setup the default number of banks.
    *  This will delete any existing banks.
    */
    void initBanks() {
      initBanks(_numBanks);
    }

    /* Setup the number of banks. 
    *  This will delete any existing banks.
    */
    void initBanks(int numBanks) {
      // Serial.println("Initialising Banks for MultiControl");
      _numBanks = numBanks;
      delete[] _bankVals;
      _bankVals = new int[_numBanks];
      for (int i=0; i<_numBanks; i++) {
        _bankVals[i] = 0;
      }
      _bankInitialised = true;
    }

    /* Choose the current bank */
    void setBank(uint8_t bank) { 
      if (!_bankInitialised) initBanks();
      _bank = bank % _numBanks; 
      _bankChanged = true;
      _latchAbove = false;
      _latchBelow = false;
    }

    /* Get the current bank */
    int getBank() { 
      if (!_bankInitialised) initBanks();
      return _bank; 
    }

    /* Set the current bank's value */
    void setBankValue(int val) { 
      if (!_bankInitialised) initBanks();
      _bankVals[_bank] = val;
    }

    /* Set a particular bank's value */
    void setBankValue(int bank, int val) { 
      if (!_bankInitialised) initBanks();
      _bankVals[bank] = val;
    }

    /* Get the current bank's value */
    int getBankValue() { 
      if (!_bankInitialised) initBanks();
      return _bankVals[_bank]; 
    }

    bool getBankInitialised() { return _bankInitialised; }

  private:

    uint8_t _pin = 0;
    int _touchValue = 0; // 0 - 1023
    int8_t _touchState = false; // 0 or 1, true is touched
    int8_t _buttonValue = false; // 0 or 1, true is pressed
    int _minTouchVal = 1024; 
    int _maxTouchVal = 0; 
    int _prevTouchVal = 0;
    uint8_t _controlType = 0; // 0 = touch, 1 = pot, 2 = button, 3 = switch
    int _potValue = 0; // 0 - 1023
    int * _potReadVals = new int[10];
    int _avePotReadVal = 0;
    uint8_t _potReadCnt = 0;
    int8_t _switchValue = 0; // 0 - 1
    const static uint8_t _TOUCH = 0;
    const static uint8_t _POT = 1;
    const static uint8_t _BUTTON = 2;
    const static uint8_t _SWITCH = 3;
    int _numBanks = 8;
    int * _bankVals;
    uint8_t _bank = 0;
    bool _bankInitialised = false;
    bool _bankChanged = true;
    bool _latchBelow = false;
    bool _latchAbove = false;
    int _firstLatchVal = -1;
    bool _firstLatchChanged = false;

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
    */
    int checkBank(int val) {
      if (_bankChanged) {
        if(_firstLatchVal == -1) _firstLatchVal = val;
        if (val != _firstLatchVal) {
          _firstLatchChanged = true;
        }
        if (!_latchAbove) {
          if (val >= getBankValue()) {
            _latchAbove = true;
          }
        }
        if (!_latchBelow) {
          if (val <= getBankValue()) {
            _latchBelow = true;
          }
        }
        if (_latchAbove && _latchBelow && _firstLatchChanged) {
          _bankChanged = false;
          _firstLatchVal = -1; // reset
          _firstLatchChanged = false;
          setBankValue(val);
        } else val = -1; // don't return anything
      } else setBankValue(val);
      return val;
    }

};

#endif /* MULTICONTROL_H_ */

