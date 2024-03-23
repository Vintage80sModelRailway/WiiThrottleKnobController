// Throttle.h
#ifndef Throttle_h
#define Throttle_h

#include <Arduino.h>

class Throttle {
  private:
  int selectorEncoderPinA = 21; // CLK pin
  int selectorEncoderPinB = 22; // DT pin
  int selectorEncoderBtn = 23; // SW pin

  int speedEncoderPinA = 21; // CLK pin
  int speedEncoderPinB = 22; // DT pin
  int speedEncoderBtn = 23; // SW pin

  int id;

  public: 
    String SelectedLocoName;
    String SelectedLocoId;
    String SelectedLocoIdType;   

    int encoderPinA_prev;
    int encoderPinA_value;
    boolean bool_CW;
    bool buttonState;

     bool goingForward;
     int knobCounter;

};


#endif
