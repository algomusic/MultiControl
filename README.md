# MultiControl
GPIO input for common controllers on the ESP32

Designed for use with the ESP32 microcontroller and the Arduino IDE. 
In particular, it is for using touch, potentiometers, buttons and switches with the OnBoard sProject PCB.
It assumes value ranges from 0 to 1023 for continuous control values (dials, touch) and 0 or 1 for on/off states (buttons, switches).

It supports bank changes and controller latching, and does its best to manage jitter and debouncing in analogue controller reads.

MultiControl is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
