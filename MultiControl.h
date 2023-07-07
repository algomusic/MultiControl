/*
 * MultiControl.h
 *
 * Manage ESP32 GPIO pins for buttons, dials, touch and switches.
 * by Andrew R. Brown 2023
 * 
 * Designed for use with the ESP32 microcontroller, the OnBoard sProject PCB, and the Arduino IDE.
 * Assumes value ranges for 0 to 1023 for continuous control values (dials, touch) and 0 or 1 for on/off states (buttons, switches).
 * 
 * MultiControl is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
 */

#ifndef MULTICONTROL_H_
#define MULTICONTROL_H_

int multiControlAnyButtonPressed = 0;

class MultiControl {
  public:
    const static uint8_t TOUCH = 0;
    const static uint8_t POT = 1;
    const static uint8_t BUTTON = 2;
    const static uint8_t SWITCH = 3;

    /** Constructor. */
    MultiControl() {};

    /** Constructor. 
    * @param pin The GPIO pin number to use for the control.
    */
    MultiControl(uint8_t pin): _pin(pin) {
      pinMode(_pin, INPUT);
      analogSetPinAttenuation(_pin, ADC_11db); // ESP32
    };

     /** Constructor. 
    * @param pin The GPIO pin number to use for the control.
    * @param controlType The type of control: 0 = touch, 1 = potentiometer, 2 = button, 3 = switch
    */
    MultiControl(uint8_t pin, uint8_t controlType): _pin(pin), _controlType(controlType) {
      pinMode(_pin, INPUT);
      analogSetPinAttenuation(_pin, ADC_11db); // ESP32
    };

    /* Set the GPIO pin to use for this control
    * @param pin The GPIO pin number to use for the control.
     */
    void setPin(uint8_t pin) { 
      _pin = pin; 
      // pinMode(_pin, INPUT);
      analogSetPinAttenuation(_pin, ADC_11db); // ESP32
    }

    uint8_t getPin() { return _pin; }

    /* Set the type of control.
    * @param controlType The type of control: 0 = touch, 1 = pot, 2 = button, 3 = switch
    */
    void setControl(uint8_t controlType) { _controlType = controlType; }

    int getControl() { return _controlType; }

    void setTouchState(bool value) { _touchState = value; } // 0 or false is off, 1 or true is on

    int getTouchState() {
      if (_touchState > 0) return 1;
      else return 0;
    }

    /* Read the touch value */
    inline
    int readTouch() {
      if (pullupOn) {
        pinMode(_pin, INPUT); // for touch
        digitalWrite(_pin, LOW); // disable internal pullup
        pullupOn = false;
      }
      int readVal = touchRead(_pin)>>8;
      if (readVal < minTouchVal) minTouchVal = readVal;
      if (readVal > maxTouchVal) maxTouchVal = minTouchVal * 5;
      int tVal = 0;
      if (readVal > minTouchVal) {
        tVal = (readVal - minTouchVal) / (float)(maxTouchVal - minTouchVal) * 1024.0f;
      }
      prevTouchVal = (prevTouchVal + tVal) * 0.5f;
      if (prevTouchVal < 10) prevTouchVal = 0;
      _touchValue = prevTouchVal;
      _touchState = prevTouchVal > 0;
      return _touchValue;
    }

    int getTouchValue() {
      return _touchValue;
    }

    inline
    bool isTouched() {
      return _touchState;
    }

    void setButtonState(bool value) { _buttonState = value; } // 0 or false is off, 1 or true is on

    /* Retrieve the current the button value 
    * @return The button value: 0 or false is off, 1 or true is on
    */
    bool getButtonState() {
      if (_buttonState > 0) return 1;
      else return 0;
    }

    /* Read the button value */
    inline
    int readButton() {
      if (!pullupOn) {
        pinMode(_pin, INPUT_PULLUP); // for buttons and switches
        pullupOn = true;
      }
      int buttonValue = digitalRead(_pin);
      if (buttonValue == 0 && _buttonState == false) {
        _buttonState = true;
        multiControlAnyButtonPressed += 1;
      }
      if (buttonValue == 1 && _buttonState == true) {
        _buttonState = false; 
        multiControlAnyButtonPressed -= 1;
      }
      return _buttonState;
    }

    bool isPressed() {
      return _buttonState;
    }

    bool isSwitchedOn() {
      return _buttonState;
    }

    /* Read the potentiometer value */
    inline
    int readPot() {
      if (pullupOn) {
        pinMode(_pin, INPUT); // for touch
        digitalWrite(_pin, LOW); // disable internal pullup
        pullupOn = false;
      }
      int readVal = analogRead(_pin) >> 2;
      if (abs(readVal - prevPotValue) > 2 || readVal == 0 || readVal == 1023) {
        prevPotValue = (prevPotValue + readVal) * 0.5f;
        _potValue = prevPotValue;
      }
      return _potValue;
    }

    /* Return the current potentiometer value */
    int getPotValue() {
      return _potValue;
    }

    /* Read the switch value */
    inline
    int readSwitch() {
      if (!pullupOn) {
        pinMode(_pin, INPUT_PULLUP); // for buttons and switches
        pullupOn = true;
      }
      _switchValue = digitalRead(_pin);
      return _switchValue;
    }

    /* Return the current switch value */
    int getSwitchValue() {
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

    /* Return the current control value based on the set type */
    int getValue() { 
      if (_controlType == 0) return _touchValue; 
      if (_controlType == 1) return _potValue;
      if (_controlType == 2) return _buttonState;
      if (_controlType == 3) return _switchValue;
      return 0; // just in case
    }

  private:

    uint8_t _pin = 0;
    int _touchValue = 0; // 0 - 1023
    bool _touchState = false; // 0 or 1, true is touched
    bool _buttonState = false; // 0 or 1, true is pressed
    int minTouchVal = 1024; 
    int maxTouchVal = 0; 
    int prevTouchVal = 0;
    unsigned long debounceTime = 0; // time of last button press
    bool pullupOn = false;  // true if internal pullup for GPIO is enabled
    uint8_t _controlType = 0; // 0 = touch, 1 = pot, 2 = button, 3 = switch
    int _potValue = 0; // 0 - 1023
    float prevPotValue = 0; // 0 - 1023
    bool _switchValue = 0; // 0 - 1

};

#endif /* MULTICONTROL_H_ */

