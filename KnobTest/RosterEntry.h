// Sensor.h
#ifndef RosterEntry_h
#define RosterEntry_h

#include <Arduino.h>

class RosterEntry {
  private:

  public: 
    String Name;
    String Id;
    String IdType;    
    int currentDirection;
    int currentControllerIndex;
    int currentSpeed;
    bool functionState[128];

};


#endif
