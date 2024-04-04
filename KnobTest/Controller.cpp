#include "Controller.h"
#include <Arduino.h>

Controller::Controller(String MTIndex, bool IsSelector, int EncoderPinA, int EncoderPinB, int EncoderBtn)
  : encoderPinA(EncoderPinA)
  , encoderPinB(EncoderPinB)
  , encoderBtn(EncoderBtn)
  , isSelector(IsSelector)
  , mtIndex(MTIndex)
{
  rosterIndex = 0;

  pinMode (encoderPinA, INPUT);
  pinMode (encoderPinB, INPUT);
  pinMode(encoderBtn, INPUT_PULLUP);
  PinA_prev = digitalRead(encoderPinA);
  ButtonState = !digitalRead(encoderBtn);

  InternalCount = 0;
  KnobPosition = -1;
  rosterIndex = -1;
  buttonIsHeld = 0;

  selectionIsChanging = -1;
  selectionChangedSinceHoldInitiated = false;

  functionSelectionChangedSinceHoldInitiated = -1;
  lastExecutedFunction = -1;
}

bool Controller::CheckMovement() {
  PinA_value = digitalRead(encoderPinA);
  bool movementDetected = false;
  int movement = 0;

  if (PinA_value != PinA_prev) {
    movementDetected = true;

    if (digitalRead(encoderPinB) != PinA_value) {
      CW = true;
      InternalCount++;
      movement++;
    } else {
      // if pin B state changed before pin A, rotation is counter-clockwise
      CW = false;
      InternalCount--;
      movement--;
    }
    
    int increase = InternalCount % 2;

    if (isSelector) {
      Serial.println("Selector move increase = " + String(increase) + "internal count " + String(InternalCount));      
      if (increase == 0) {
        if (CW) {
          KnobPosition++;
          Serial.println(mtIndex + " CW movement " + String(KnobPosition));
        } else {
          KnobPosition--;
          Serial.println(mtIndex + " AC movement " + String(KnobPosition));
        }
      }
      else {
        Serial.println ("No increase internal "+String(InternalCount)+" inc "+String(increase)+" kp "+String(KnobPosition));
      }
    }
    else {
      if (CW) {
        KnobPosition++;
        Serial.println(mtIndex + " CW movement " + String(KnobPosition));
      } else {
        KnobPosition--;
        Serial.println(mtIndex + " AC movement " + String(KnobPosition));
      }
    }

    if (KnobPosition < -1) KnobPosition = -1;
    if (KnobPosition > 126) KnobPosition = 126;

    if (InternalCount < -32000) InternalCount = -1;
    if (InternalCount > 32000) InternalCount = -1;
  }
  PinA_prev = PinA_value;

  // check if button is pressed (pin SW)
  bool newButtonState = !digitalRead(encoderBtn);
  if (newButtonState != ButtonState) {
    buttonIsHeld = 0;
    millisAtButtonPress = millis();
    if (newButtonState == HIGH) {
      //Serial.println("Button Pressed");
      ButtonState = 1;
      //Serial.println("Button change pressed");
    }
    else {
      //Serial.println("Button Released");
      ButtonState = 0;
      //Serial.println("Button change released");
    }
  }
  else {
    if (ButtonState == 1 && (millis() - millisAtButtonPress > 1000) && buttonIsHeld == 0) {
      buttonIsHeld = 1;
      Serial.println("Button held");
    }
  }






  return movementDetected;
}
