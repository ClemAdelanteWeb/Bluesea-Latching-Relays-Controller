/*
  Relay Library for controlling Latching Relays on a Marine or Home battery system.
  Created by Clément Lambert, 6th march 2018.
  Released into the public domain.
*/

#include "Relay.h"

/**
 * Constructor
 */
Relay::Relay() {
    Serial.println(this->state);
}

void Relay::setClosed() {
    if(this->getState() != 1) {
        analogWrite(this->closePin, 255);
        Serial.print(this->name);
        Serial.println(" closing relay");
        delay(latchingDurationTime);
        Serial.println("Relay closed");
        analogWrite(this->closePin, 0);
        
        this->state = 1;
    }
}


void Relay::setOpened() {
    Serial.print(this->name);
    Serial.print(" set opened ");
    Serial.println(this->state);
    
    if(this->getState() != 0) {
        analogWrite(this->openPin, 255);
        Serial.print(this->name);
        Serial.println(" opening relay");
        delay(latchingDurationTime);
        Serial.println("Relay opened");
        analogWrite(this->openPin, 0);
        
        this->state = 0;
    }
}

byte Relay::getState() {
    return this->state;
}
