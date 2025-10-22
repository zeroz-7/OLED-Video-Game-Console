# OLED-Video-Game-Console
Repo containing code and manul for Video Game Console using ESP8266 NodeMCU, 0.96 OLED Display, Analogue Joystick, and 4 buttons with a breadboard for assist. The two games available are RoboDodge and TicTacToe.
####updates v1( updated by iron-man) ####
removed joystick, control is only through 4 buttons.
**there are 2 games**, 
  1. robo-dodge
  2. tic-tac-toe
The main code is at the location ```OLED-Video-Game-Console/src/main.cpp```
project uses plarform-io for development so the code imports ```#include <Arduino.h>``` in the first line as platform-io needs this in order to specify the compiler that the code is in Arduino framework. When uploading the code through Arduino-IDE this line can be deleted and uploaded without any issues.

**LIBRARIES TO BE INSTALLED**
 1. Adafruit GFX Library
 2. Adafruit SSD1306
