/*
  BlueSeaLatchingRelay.h - Library for controlling Latching Relays on a Marine or Home battery system.
  Created by Clément Lambert, 6th march 2018.
  Released into the public domain.
*/

#ifndef BlueSeaLatchingRelay_H
#define BlueSeaLatchingRelay_H

#include "Arduino.h"

class BlueSeaLatchingRelay {
  
  public :
    BlueSeaLatchingRelay();
    byte state = 22;
    byte openPin;
    byte closePin;
    byte statePin;
    String name;
    
    // Opening or closing time in ms
    int latchingDurationTime = 500;
    
    const byte RELAY_OPENED = 1;
    const byte RELAY_CLOSED = 0;
    
    void setClosed();
    
    void setOpened();
  
    byte getState();

};

#endif
