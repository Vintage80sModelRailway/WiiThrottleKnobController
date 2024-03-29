# WiiThrottleKnobController

A sketch written for ESP32 in the Arduino environment that drives locomotives using attached KY-040 rotary encoders through JMRI's WiiThrottle protocol.

Requires this library https://www.arduino.cc/reference/en/libraries/liquidcrystal-i2c/

Things to change if you're going to have any chance of making it work for you

   - on wifi.Connect() change the IP address and port to those of your WiiThrottle server
   - #define NUMBER_OF_THROTTLES at the top - set it to the number of THROTTLES you have (not the total number of knobs)
   - RosterEntry roster[x] where x >= the number of locos that will be sent from WiiThrottle to your device at startup
   - selector = Controller("AS", true, 34, 39, 36) - this cluster of lines sets up your knobs and what pins they're on (there's a bit more in the controller.h class definition)
   - LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, 20 column and 4 rows - change here if using a different address. The code is entirely designed to use 20x04, nothing else will work without changing the code
   - wifiClient.write("NESP32 Knob Throttle\n"); - customise the name that's shown in WiiThrottle, ie ("NMy new throttle\n") (Leave the N and \n in there)
   - add an arduino_secrets.h file to the project, or replace the lines const char* ssid = SECRET_SSID; (and the password one) with const char* ssid = "my SSID";
   - if you go for direct copy and paste of SSID and password, remove the #include "arduino_secrets.h" reference at the top of the sketch

It really needs more comments and sorting out, I just haven't got around to it. Some tips, until I do get it properly tidied up. mtIndex = multiThrottle Index. Just a unique ID used to identify the multi throttle in WiiThrottle. I just used 1,2,3,4,5 - and they're the values used on screen, ie "Throttle 1"
