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
    byte state = NULL;
    byte openPin;
    byte closePin;
    byte statePin;
    byte isReadyToOpen = 0;
    byte isReadyToClose = 0;
    byte isForceToOpen = 0;
    byte isForceToClose = 0;
 
    
    String name;
    
    // Opening or closing time in ms
    int latchingDurationTime = 600;
    
    // Relay opened, no current flowing
    const byte RELAY_OPEN = 0;
    
    // Relay closed, current is flowing
    const byte RELAY_CLOSE = 1;
    
    void setClosed();
    
    void setOpened();
    
    void setReadyToClose();
    
    void setReadyToOpen();

    void forceToOpen();
    
    void forceToClose();
    
    void applyReadyActions();
        
    // Determine the beginning of a new cycle (run)
    // reset all "ready" states
    void startCycle();
  
    byte getState();

};

#endif

