// Arduino and KY-040 module
#include <WiFiClientSecure.h>
#include <LiquidCrystal_I2C.h>
#include "RosterEntry.h"
#include "Controller.h"
#include "arduino_secrets.h"

#define NUMBER_OF_THROTTLES 5

String prefix;
String displayText[4];
String locoName;
String delimeter = "";
String receivedLine = "";
int locoArrayCounter = 0;
int rosterCounter = 0;
bool rosterCollecting = false;
bool actionCollecting = false;
bool trackPowerCollecting = false;
bool trackPowerState = false;
String actionReceivedLine = "";
String actionControllerIndex = "";
int numberOfLocosInRoster = -1;
RosterEntry currentLoco;
RosterEntry roster[30];
Controller selector;
Controller functionSelector;
Controller throttles[NUMBER_OF_THROTTLES];
bool statusPageScrolledDown = false;
bool screenMessageTImeoutActive = false;
unsigned long startOfTimeout;
int timeoutLength;

const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASS;


WiFiClient wifiClient;
LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27, 20 column and 4 rows

void setup() {
  Serial.begin (19200);
  WiFi.begin(ssid, password);

  selector = Controller("AS", true, 34, 39, 36);
  functionSelector = Controller("FS", true, 18, 19, 23);
  throttles[0] = Controller("1", false,  16, 17, 5);
  throttles[1] = Controller("2", false,  13, 15, 14);
  throttles[2] = Controller("3", false,  33, 32, 35);
  throttles[3] = Controller("4", false,  2, 0, 4);
  throttles[4] = Controller("5", false,  27, 26, 25);

  lcd.init();
  lcd.backlight();

  int tryDelay = 500;
  int numberOfTries = 20;

  bool isConnected = false;
  scrollToScreen("Wifi Connect");

  while (!isConnected) {

    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        Serial.println("[WiFi] SSID not found");
        break;
      case WL_CONNECT_FAILED:
        Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
        return;
        break;
      case WL_CONNECTION_LOST:
        Serial.println("[WiFi] Connection was lost");
        break;
      case WL_SCAN_COMPLETED:
        Serial.println("[WiFi] Scan is completed");
        break;
      case WL_DISCONNECTED:
        Serial.println("[WiFi] WiFi is disconnected");
        break;
      case WL_CONNECTED:
        Serial.println("[WiFi] WiFi connected!");
        scrollToScreen("WiFi is connected");
        Serial.print("[WiFi] IP address: ");
        //String ip = "";// WiFi.localIP();
        //scrollToScreen("IP address: "+ip);
        Serial.println(WiFi.localIP());

        isConnected = true;
        break;
      default:
        Serial.print("[WiFi] WiFi Status: ");
        Serial.println(WiFi.status());
        //scrollToScreen("WiFi Status: "+WiFi.status());
        break;
    }
    delay(tryDelay);

    if (numberOfTries <= 0) {
      Serial.print("[WiFi] Failed to connect to WiFi!");
      WiFi.disconnect();
      return;
    } else {
      numberOfTries--;
    }
  }

  if (!wifiClient.connect(IPAddress(192, 168, 1, 29), 12090)) {
    Serial.println("Connection to host failed");
  }
  else {
    wifiClient.write("NESP32 Knob Throttle\n");
  }
  prefix = "";
  locoName = "";
}

void loop() {

  while (wifiClient.available() && wifiClient.available() > 0) {
    readFromWiiTHrottle();
  }

  checkForDisplayTimeout();
  int previousSelectorKnobPosition = selector.KnobPosition;
  bool previousSelectorButtonState = selector.ButtonState;
  selector.CheckMovement();

  if (previousSelectorKnobPosition != selector.KnobPosition) {
    if (selector.KnobPosition >= 0 && selector.KnobPosition < numberOfLocosInRoster) {
      if (selector.selectionIsChanging > -1) {
        //Found throttle that wants to change assignment

        int s = selector.selectionIsChanging;
        //send current selection to screen?
        String currentlySelectedTrainName = roster[selector.KnobPosition].Name;
        Serial.println("selector change detected - assigned controller is " + String(s) + " current roster entry " + String(previousSelectorKnobPosition) + " - new selection - "  + String(selector.KnobPosition) + " - " + currentlySelectedTrainName);
        throttles[s].selectionChangedSinceHoldInitiated = 1;
        printLineToScreen(currentlySelectedTrainName, 0, 2);
      }
      else {
        //Selector turning but nothing held down - cycle screen?
        Serial.println("Selector move but nothing selected position " + String(selector.KnobPosition));
        if (selector.KnobPosition < 0) selector.KnobPosition = 0;
        if (selector.KnobPosition > NUMBER_OF_THROTTLES - 1) selector.KnobPosition = NUMBER_OF_THROTTLES - 1;
        displaySummaryPageForThrottle(selector.KnobPosition);
      }
    }
    else if (selector.KnobPosition >= numberOfLocosInRoster) {
      selector.KnobPosition = numberOfLocosInRoster - 1;
    }
    else if (selector.KnobPosition < -1) {
      selector.KnobPosition = 0;
    }
  }
  if (previousSelectorButtonState != selector.ButtonState) {
    if (selector.ButtonState == 0) {
      //Button released
      //Serial.println("Power state previously " + String(trackPowerState));
      trackPowerState = !trackPowerState;
      writeString("PPA" + String(trackPowerState) + "\n"); //send power instruction to WT
      //Serial.println("Power state request to " + String(trackPowerState));
    }
  }

  int previousFunctionKnobPosition = functionSelector.KnobPosition;
  bool previousFunctionButtonState = functionSelector.ButtonState;
  int s = functionSelector.selectionIsChanging;
  String m = throttles[s].mtIndex;
  int t = throttles[s].rosterIndex;

  functionSelector.CheckMovement();
  if (previousFunctionKnobPosition != functionSelector.KnobPosition)  {
    if (functionSelector.selectionIsChanging >= 0) {
      //Serial.println("Function selector movement s " + String(s) + " m " + String(m) + " t " + String(t));
      if (functionSelector.KnobPosition >= 0 && functionSelector.KnobPosition < 128) {
        //Found throttle that wants to send function
        Serial.println("Function selector change detected - assigned controller is " + String(s) + " current roster entry " + String(t) + " - function selection - "  + String(functionSelector.KnobPosition));
        throttles[s].functionSelectionChangedSinceHoldInitiated = 1;
        String state = "Off";
        if (roster[t].functionState[functionSelector.KnobPosition] == 1) {
          state = "On";
        }
        printLineToScreen("F" + String(functionSelector.KnobPosition) + " (" + state + ")", 0, 2);
      }
      else {
        functionSelector.KnobPosition = 0;
      }
    }
    else {
      //Scroll?
      if (NUMBER_OF_THROTTLES == 5) {
        if (functionSelector.KnobPosition > previousFunctionKnobPosition) {
          statusPageScrolledDown = true;
          Serial.println("F scroll down");
        }
        else {
          statusPageScrolledDown = false;
          Serial.println("F scroll up");
        }
        showStatusPage();
      }
    }
  }
  if (functionSelector.ButtonState != previousFunctionButtonState)
    if (functionSelector.selectionIsChanging >= 0) {
      //function send required on press and release

      String func = "M" + m + "A" + roster[t].IdType + roster[t].Id + "<;>F" + String(functionSelector.ButtonState) + String(functionSelector.KnobPosition) + "\n";
      writeString(func);
      Serial.print("Funtion sent " + func);
      throttles[s].functionSelectionChangedSinceHoldInitiated = 1;
      throttles[s].lastExecutedFunction = functionSelector.KnobPosition;
      printLineToScreen("Function " + String(functionSelector.KnobPosition) + " set", 0, 2);
      freezeScreen(5000);
    }
    else {
      if (functionSelector.ButtonState == 0) {
        Serial.println("button press but no function changing - stop all throttles");
        for (int i = 0; i < NUMBER_OF_THROTTLES; i++) {
          if (throttles[i].rosterIndex < 0) continue;
          String spd = "M" + throttles[i].mtIndex + "A" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>V0\n";
          Serial.print("Stop train - New speed 0 writing " + spd);
          throttles[i].KnobPosition = 0;
          roster[throttles[i].rosterIndex].currentSpeed = 0;
          writeString(spd);
          updateSpeedOnLCD(i);
        }
      }
    }

  for (int i = 0; i < NUMBER_OF_THROTTLES; i++) {
    int previousSpeed = throttles[i].KnobPosition;
    bool previousButtonState = throttles[i].ButtonState;
    bool previousButtonHeldState = throttles[i].buttonIsHeld;

    throttles[i].CheckMovement();

    if (previousSpeed != throttles[i].KnobPosition) {
      roster[throttles[i].rosterIndex].currentSpeed = throttles[i].KnobPosition;
      if (throttles[i].rosterIndex >= 0 && throttles[i].KnobPosition >= 0) {
        String spd = "M" + throttles[i].mtIndex + "A" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>V" + String(throttles[i].KnobPosition) + "\n";
        Serial.print("New speed " + String(throttles[i].KnobPosition) + " writing " + spd);
        writeString(spd);
        updateSpeedOnLCD(i);
      }
    }

    //Stop / direction change

    if (throttles[i].ButtonState != previousButtonState && throttles[i].ButtonState == 0 && previousButtonHeldState != 1) {// && functionSelector.selectionIsChanging <0 && functionSelector.selectionIsChanging <0) { //buttonIsHeld has not yet been processed so this check will think it's held / been held
      //Serial.println(String(i) + " pBHS " + String(previousButtonHeldState) + " c - " + String(throttles[i].buttonIsHeld));
      if (throttles[i].rosterIndex > -1)
      {
        if (throttles[i].KnobPosition > 0) {
          String spd = "M" + throttles[i].mtIndex + "A" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>V0\n";
          Serial.print("Train moving, emergency stop here - New speed 0 writing " + spd);
          throttles[i].KnobPosition = 0;
          roster[throttles[i].rosterIndex].currentSpeed = 0;
          writeString(spd);
          updateSpeedOnLCD(i);
        }
        else
        {
          bool oldDir = roster[throttles[i].rosterIndex].currentDirection;
          roster[throttles[i].rosterIndex].currentDirection = !oldDir;
          String dir =  "M" + throttles[i].mtIndex + "A" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>R" + String(roster[throttles[i].rosterIndex].currentDirection) + "\n";
          Serial.print("OK to change direction from " + String(oldDir) + " to " + String(roster[throttles[i].rosterIndex].currentDirection) + " - " + dir);
          writeString(dir);
          showStatusPage();
        }
      }
    }

    //Change selection
    if (previousButtonHeldState != throttles[i].buttonIsHeld) {
      if (throttles[i].buttonIsHeld == 1) {
        Serial.println("Selector button held - throttle " + String(i) + " current train assignment " + throttles[i].TrainName + " - " + String(throttles[i].rosterIndex));
        selector.selectionIsChanging = i;
        functionSelector.selectionIsChanging = i;
        selector.KnobPosition = throttles[i].rosterIndex;
        functionSelector.KnobPosition = throttles[i].lastExecutedFunction;
        clearScreenAndDisplayCache();
        printLineToScreen("Throttle " + throttles[i].mtIndex, 0, 0);
        if (throttles[i].rosterIndex >= 0) {
          printLineToScreen("Select loco or fn", 0, 1);
        }
        else {
          printLineToScreen("Select loco to start", 0, 1);
        }

      }
      else { //hold has been released
        Serial.println("Hold released on throttle " + String(i));
        if (throttles[i].selectionChangedSinceHoldInitiated == 1) {
          //deselect train from MT
          throttles[i].TrainName = "";
          throttles[i].rosterIndex = -1;
          if (selector.KnobPosition > -1) {
            Serial.println("Selector button hold released - assign currently selected train - " + String(selector.KnobPosition));
            throttles[i].TrainName = roster[selector.KnobPosition].Name;
            throttles[i].rosterIndex = selector.KnobPosition;

            String rel = "M" + throttles[i].mtIndex + "-" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>r\n";
            Serial.print("Released else Write " + rel);
            writeString(rel);
            String assign = "M" + throttles[i].mtIndex + "+" + roster[selector.KnobPosition].IdType +  roster[selector.KnobPosition].Id + "<;>" + roster[selector.KnobPosition].IdType +  roster[selector.KnobPosition].Id + "\n";
            Serial.print("Assigned Write " + assign);
            writeString(assign);
            String confirmMessage[4];
            confirmMessage[0] = "Throttle " + throttles[i].mtIndex;
            confirmMessage[1] = throttles[i].TrainName;
            confirmMessage[2] = "Addr " + roster[selector.KnobPosition].Id + roster[selector.KnobPosition].IdType;
            confirmMessage[3] = "Acquired";

            displayTimeoutMessage(confirmMessage, 5000);
            selector.KnobPosition = -1;
          }
        }
        if (throttles[i].functionSelectionChangedSinceHoldInitiated ) {
          //throttles[i].functionSelectionChangedSinceHoldInitiated = 0;
        }
        if (!throttles[i].functionSelectionChangedSinceHoldInitiated && !throttles[i].selectionChangedSinceHoldInitiated) {
          selector.selectionIsChanging = -1;
          Serial.println("Selector button hold released - no change in selection detected, no new assignment - " + String(selector.KnobPosition));
          //deselect train from MT
          String confirmMessage[4];
          confirmMessage[0] = "Throttle " + throttles[i].mtIndex;
          confirmMessage[1] = "Released";
          confirmMessage[2] = "";
          confirmMessage[3] = "";

          displayTimeoutMessage(confirmMessage, 5000);

          if (selector.KnobPosition > -1) {
            String rel = "M" + throttles[i].mtIndex + "-" + roster[throttles[i].rosterIndex].IdType + roster[throttles[i].rosterIndex].Id + "<;>r\n";
            Serial.print("Write " + rel);
            writeString(rel);
          }
          throttles[i].TrainName = "";
          throttles[i].rosterIndex = -1;
          selector.KnobPosition = -1;
        }

        throttles[i].selectionChangedSinceHoldInitiated = 0;
        throttles[i].functionSelectionChangedSinceHoldInitiated = 0;
        selector.selectionIsChanging = -1;
        functionSelector.selectionIsChanging = -1;
      }
    }
  }
}

void writeString(String stringData) { // Used to serially push out a String with Serial.write()

  for (int i = 0; i < stringData.length(); i++)
  {
    wifiClient.write(stringData[i]);   // Push each char 1 by 1 on each loop pass
  }

}// end writeString

void scrollToScreen(String line) {
  //lcd.setCursor(0, 0);
  //lcd.print("Hello");
  //return;
  for (int i = 0; i < 4; i++) {
    if (i < 3) {
      displayText[i] = displayText[i + 1];
    }
    else {
      displayText[i] = line;
    }

    String printLine = displayText[i];

    int len = printLine.length();
    int padding = 20 - len;
    for (int p = 1; p <= padding; p++) {
      printLine += " ";
    }

    lcd.setCursor(0, i);
    lcd.print(printLine);
  }
}

void clearScreenAndDisplayCache() {
  lcd.clear();
  for (int i = 0; i < 4; i++) {
    displayText[i] = "";
  }
}

void printLineToScreen(String line, int x, int y) {
  lcd.setCursor(0, y);
  lcd.print("                    ");
  lcd.setCursor(x, y);
  lcd.print(line.substring(0, 20));
}

void showStatusPage() {
  lcd.clear();
  int first = 0;
  int scrollIncrement = 0;
  if (statusPageScrolledDown && NUMBER_OF_THROTTLES == 5) {
    first = 1;
    scrollIncrement = 1;
  }

  //handle scrolling on 4 line display with 5 throttles
  int last = NUMBER_OF_THROTTLES;
  if (NUMBER_OF_THROTTLES == 5) {
    if (statusPageScrolledDown) {
      last = 5;
    }
    else {
      last = 4;
    }
  }

  bool activeThrottle = false;
  String powerState = "ON";
  int powerCursorStart = 17;
  if (trackPowerState == 0) {
    powerState = "OFF";
    powerCursorStart = 16;
  }

  //just in case throttle 1 is alone occupied and page is scrolled down
  if (throttles[0].rosterIndex >= 0) activeThrottle = true;

  Serial.println("Num throttles: " + String(NUMBER_OF_THROTTLES) + " first " + String(first) + " last " + String(last));
  for (int i = 0; i < 4; i++) {
    int adjustedThrottleIndex = i + scrollIncrement;
    if (adjustedThrottleIndex >= NUMBER_OF_THROTTLES) continue;
    if (throttles[adjustedThrottleIndex].rosterIndex < 0) {
      String uLine = throttles[adjustedThrottleIndex].mtIndex + ": Unassigned      ";
      lcd.setCursor(0, i);
      lcd.print(uLine);
      continue;
    }
    activeThrottle = true;
    lcd.setCursor(0, i);
    String line = "";
    line += throttles[adjustedThrottleIndex].mtIndex + ":";
    line += roster[throttles[adjustedThrottleIndex].rosterIndex].Id;
    String dir = "F";
    if (roster[throttles[adjustedThrottleIndex].rosterIndex].currentDirection < 1) {
      dir = "R";
    }
    line += " D:" + dir;
    line += " S:" + String(roster[throttles[adjustedThrottleIndex].rosterIndex].currentSpeed);
    Serial.println("Status line " + line);
    lcd.print(line.substring(0, 20));
    if (i == 0) {
      lcd.setCursor(powerCursorStart, i);
      lcd.print("P" + powerState);
    }
  }
  if (!activeThrottle) {
    printLineToScreen("Connected", 0, 0);
    printLineToScreen(String(numberOfLocosInRoster) + " locos in roster", 0, 1);
    printLineToScreen("Track Power " + powerState, 0, 2);
    printLineToScreen("", 0, 3);
  }
}

void updateSpeedOnLCD(int throttleIndex) {
  if (screenMessageTImeoutActive) return;
  String line = "";
  line += throttles[throttleIndex].mtIndex + ":";
  line += roster[throttles[throttleIndex].rosterIndex].Id;
  String dir = "F";
  if (roster[throttles[throttleIndex].rosterIndex].currentDirection < 1) {
    dir = "R";
  }
  line += " D:" + dir;
  line += " S:";
  int lineIndex = throttleIndex;
  if (NUMBER_OF_THROTTLES >= 5) {
    if (statusPageScrolledDown) {
      lineIndex = throttleIndex - 1;
      if (throttleIndex == 0) return;
    }
    else {
      lineIndex = throttleIndex;
      if (throttleIndex == 4) return;
    }
  }
  int x = line.length();
  lcd.setCursor(x, lineIndex);
  String spd = String(roster[throttles[throttleIndex].rosterIndex].currentSpeed);
  String wholeLine = line + spd;
  int len = wholeLine.length();
  int padding = 16 - len;
  String pd = "";
  for (int p = len; p < 16; p++) {
    pd += " ";
  }
  lcd.print (spd + pd);
}

void displayTimeoutMessage(String message[4], int displayFor) {
  lcd.clear();
  for (int i = 0; i < 4; i++) {
    lcd.setCursor(0, i);
    lcd.print(message[i].substring(0, 20));
  }
  screenMessageTImeoutActive = true;
  startOfTimeout = millis();
  timeoutLength = displayFor;
}

void freezeScreen(int displayFor) {
  screenMessageTImeoutActive = true;
  startOfTimeout = millis();
  timeoutLength = displayFor;
}

void checkForDisplayTimeout() {
  if (!screenMessageTImeoutActive) return;
  if (millis() - startOfTimeout >= timeoutLength) {
    bool throttleHeld = false;
    for (int i = 0; i < NUMBER_OF_THROTTLES; i++) {
      if (throttles[i].buttonIsHeld) {
        throttleHeld = true;
      }
    }
    if (!throttleHeld) {
      showStatusPage();
      screenMessageTImeoutActive = false;
    }

  }
}

void displaySummaryPageForThrottle(int throttleIndex) {
  if (throttleIndex > NUMBER_OF_THROTTLES - 1 || throttleIndex < 0) return;

  lcd.clear();
  String line1 = "Throttle " + throttles[throttleIndex].mtIndex;
  String line2 = "                    ";
  String line3 = "                    ";
  String line4 = "                    ";

  if (throttles[throttleIndex].rosterIndex < 0) {
    line2 = "Unassigned";
  }
  else {
    line2 = roster[throttles[throttleIndex].rosterIndex].Name;
    line3 = "Addr " + roster[throttles[throttleIndex].rosterIndex].Id + roster[throttles[throttleIndex].rosterIndex].IdType;
  }
  printLineToScreen(line1, 0, 0);
  printLineToScreen(line2, 0, 1);
  printLineToScreen(line3, 0, 2);
  printLineToScreen(line4, 0, 3);
  freezeScreen(5000);
}

void readFromWiiTHrottle() {
  char x = wifiClient.read();
  prefix += x;
  receivedLine += x;
  if (trackPowerCollecting == true) {
    if (x == '\n') {
      //Serial.println(receivedLine);
      String cTrackPowerState = receivedLine.substring(3, 4);
      //Serial.println("Track power message "+cTrackPowerState+" - ");
      if (cTrackPowerState == "1") {
        trackPowerState = true;
      }
      else {
        trackPowerState = false;
      }
      trackPowerCollecting = false;
      //Serial.println("Track power state now "+String(trackPowerState));
      showStatusPage();
    }
  }
  if (actionCollecting == true) {
    if (x == '\n') {
      //Serial.println("WT action: " + actionReceivedLine);

      int index = actionReceivedLine.indexOf("<;>");
      String firstHalf = actionReceivedLine.substring(0, index);
      // Serial.println("Firsthalf "+firstHalf);

      int addressIndex = firstHalf.indexOf("AS");
      if (addressIndex == -1) {
        addressIndex = firstHalf.indexOf("AL");
      }
      String throttleIndex = firstHalf.substring(1, addressIndex);
      int iThrottleIndex = throttleIndex.toInt();
      //Serial.println("Controller index "+controllerIndex);
      String address = firstHalf.substring(addressIndex + 1);
      //Serial.println("Address "+address);
      String secondHalf = actionReceivedLine.substring(index + 3);
      //Serial.println("Secondhalf "+secondHalf);

      String actionType = secondHalf.substring(0, 1);
      String actionInstruction = secondHalf.substring(1);

      int rosterIndexToUpdate = -1;
      for (int r = 0; r < numberOfLocosInRoster; r++) {
        String locoAddress = roster[r].IdType + roster[r].Id;
        if (locoAddress == address) {
          rosterIndexToUpdate = r;
          //Serial.println("Matched loco " + roster[r].Name);
          break;
        }
      }
      if (actionType == "V") {
        //velocity/speed
        if (throttles[iThrottleIndex - 1].rosterIndex > -1) {
          int originalKnobPosition = throttles[iThrottleIndex - 1].KnobPosition;
          int newSpeed = actionInstruction.toInt();
          int diff = newSpeed -  roster[rosterIndexToUpdate].currentSpeed;
          if (diff > 5 || diff < -5)
          {
            roster[rosterIndexToUpdate].currentSpeed = actionInstruction.toInt();
            //Serial.println("Set " + roster[rosterIndexToUpdate].Name + " to speed " + String(roster[rosterIndexToUpdate].currentSpeed));
            //Serial.println("Action type " + actionType + " instruction " + actionInstruction + " controller " + String(throttleIndex));
            if (throttles[iThrottleIndex - 1].rosterIndex == rosterIndexToUpdate) {
              throttles[iThrottleIndex - 1].KnobPosition = roster[rosterIndexToUpdate].currentSpeed;
              Serial.println("Set active controller roster knob speed");
            }
            int diff = originalKnobPosition -  roster[rosterIndexToUpdate].currentSpeed;
            if (!screenMessageTImeoutActive && (diff > 10 || diff < -10)) {
              //showStatusPage();
              updateSpeedOnLCD(iThrottleIndex - 1);
            }
          }

        }

      }
      else if (actionType == "R") {
        //direction
        roster[rosterIndexToUpdate].currentDirection = actionInstruction.toInt();
        //Serial.println("Set " + roster[rosterIndexToUpdate].Name + " to direction " + String(roster[rosterIndexToUpdate].currentDirection));
        if (!screenMessageTImeoutActive) {
          showStatusPage();
        }
      }
      else if (actionType == "X") {
        //emergency stop
        roster[rosterIndexToUpdate].currentSpeed = 0;
        String spd = "M" + throttleIndex + "A*<;>V0\n";
        writeString(spd);
      }
      else if (actionType == "F") {
        //function
        String functionState = actionInstruction.substring(0, 1);
        String functionAddress = actionInstruction.substring(1);

        int iFunctionState = functionState.toInt();
        int iFunctionAddress = functionAddress.toInt();

        // Serial.println("Function - " + roster[rosterIndexToUpdate].Name + "storing state " + functionState + " for address " + functionAddress);
        roster[rosterIndexToUpdate].functionState[iFunctionAddress] = iFunctionState;
      }

      actionReceivedLine = "";
    }
    actionReceivedLine += x;
    if (actionReceivedLine.length() == 1)
    {
      actionControllerIndex = x;

    }
  }
  if (rosterCollecting == true) {
    if (x == '{' || x == '|' || x == '[' || x == '}' || x == ']' || x == '\\' || x == '\n') {
      delimeter += x;
      if (delimeter == "]\\[" || x == '\n') {
        //new loco
        //Serial.println("Loco " + locoName);
        if (numberOfLocosInRoster < 0) {
          numberOfLocosInRoster = locoName.toInt();
          Serial.println ("Number of locos " + locoName);
          scrollToScreen("Number of locos: " + locoName);
          locoName = "";
        }
        else {
          //Serial.println ("Idtype " + locoName);
          currentLoco.IdType = locoName;
          locoName = "";
          locoArrayCounter = 0;
          roster[rosterCounter] = currentLoco;
          rosterCounter++;
        }

      }
      else if (delimeter == "}|{") {
        if (locoArrayCounter == 0) {
          //Serial.println ("Name " + locoName);
          currentLoco.Name = locoName;
          locoName = "";
        }
        else if (locoArrayCounter == 1) {
          //Serial.println ("Id " + locoName);
          currentLoco.Id = locoName;
          locoName = "";
        }
        else if (locoArrayCounter == 2 ) {

          locoName = "";
        }
        locoArrayCounter++;
        //Serial.println(delimeter);
      }
    }
    else {
      delimeter = "";
      locoName += x;
    }
  }
  if (x == '\n') {
    //Serial.print("Nreline");
    prefix = "";
    if (rosterCollecting) {
      //Serial.println("Roster complete");

    }

    rosterCollecting = false;
    actionReceivedLine = "";
    actionCollecting = false;
    trackPowerCollecting = false;
    receivedLine = "";
  }
  if (prefix == "RL") {
    //Serial.println("Roster line");
    rosterCollecting = true;
  }
  if (prefix == "M") {
    //Serial.println("Incoming action");
    actionCollecting = true;
    actionReceivedLine += x;
  }
  if (prefix == "PPA") {
    Serial.println("Track powerr prefix " + prefix);
    trackPowerCollecting = true;
  }
  //Serial.print(x);

}
