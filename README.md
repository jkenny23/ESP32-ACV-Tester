# ESP32-ACV-Tester
ESP32 automatic cell voltage tester. Detects a cell (voltage >200mV) and types the cell voltage via BLE keyboard connection on a computer or phone.

## Dependencies
To build this project, you will need Arduino IDE, ESP32 in Arduino, and the following 2 libraries:
https://github.com/T-vK/ESP32-BLE-Keyboard
https://github.com/adafruit/Adafruit_ADS1X15

## Calibration
Open a serial port to the ESP32, baud rate 115200. Insert a cell, wait for voltage to settle. Measure the real cell voltage with an accurate voltmeter. Calculate the correction factor = (cell voltage measured by ESP32) / (cell voltage measured by voltmeter) * 10000. Enter this value (e.g. 10029) in the following console string: tw a0 d10029, where 10029 is replaced by the value you calculated in the last step. 
