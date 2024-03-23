// Controller.h
#ifndef Controller_h
#define Controller_h

#include <Arduino.h>

class Controller {
  private:
    int encoderPinA; // CLK pin
    int encoderPinB; // DT pin
    int encoderBtn; // SW pin

    int PinA_prev;
    int PinA_value;
    bool CW;   
    bool isSelector;
    int InternalCount;
    int id;
    unsigned long millisAtButtonPress;

  public:
    //selector
    int selectionIsChanging;
    
    //throttle / both
    String TrainName;
    int rosterIndex;
    String mtIndex;
    bool ButtonState;
    bool buttonIsHeld;   
    bool selectionChangedSinceHoldInitiated; 
    bool functionSelectionChangedSinceHoldInitiated;
    int lastExecutedFunction;
    int KnobPosition;
    
    
    Controller(String MTIndex = "", bool IsSelector = false, int EncoderPinA = -1, int EncoderPinB  = -1, int EncoderBtn  = -1);
    bool CheckMovement();
};


#endif
