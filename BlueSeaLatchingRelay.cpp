/*
  Relay Library for controlling Latching Relays on a Marine or Home battery system.
  Created by Clément Lambert, 6th march 2018.
  Released into the public domain.
*/

#include "BlueSeaLatchingRelay.h"

/**
 * Constructor
 */
BlueSeaLatchingRelay::BlueSeaLatchingRelay() {
    
}

void BlueSeaLatchingRelay::setClosed() {
	this->isReadyToOpen = 0;

    if(this->getState() != BlueSeaLatchingRelay::RELAY_CLOSE) {
    
        analogWrite(this->closePin, 255);
        
        Serial.print(this->name);
        Serial.println(F(" closing"));
        
        delay(latchingDurationTime);
        
        analogWrite(this->closePin, 0);
        
        // delay to be sure that 2 relays can't be activated at the same time (causing fuse blow)
        delay(800);
             
        if(this->getState() == BlueSeaLatchingRelay::RELAY_CLOSE) {
        	Serial.println(F("R. closed"));
        } else {
        	Serial.println(F("Error, can't close r."));
        }
        
        this->state = BlueSeaLatchingRelay::RELAY_CLOSE;
    }
}


void BlueSeaLatchingRelay::setOpened() {
    
    if(this->getState() != BlueSeaLatchingRelay::RELAY_OPEN) {
    
        analogWrite(this->openPin, 255);
        
        Serial.print(this->name);
        Serial.println(F(" opening"));
        
        delay(latchingDurationTime);
        
        analogWrite(this->openPin, 0);
        
        // delay to be sure that 2 relays can't be activated at the same time (causing fuse blow)
        delay(800);
             
        if(this->getState() == BlueSeaLatchingRelay::RELAY_OPEN) {
        	Serial.println(F("r. opened"));
        } else {
        	Serial.println(F("Error, can't open r."));
        	// Serial.print("openPin : ");
        	// Serial.println(this->openPin);
        }
        
        this->state = BlueSeaLatchingRelay::RELAY_OPEN;
    }
}

void BlueSeaLatchingRelay::setReadyToClose() {
	this->isReadyToClose = 1;
}

void BlueSeaLatchingRelay::setReadyToOpen() {
	this->isReadyToOpen = 1;
}

void BlueSeaLatchingRelay::forceToClose() {
	this->isForceToClose = 1;
	this->setClosed();
}
void BlueSeaLatchingRelay::forceToOpen() {
	this->isForceToOpen = 1;
	this->setOpened();
}

/*
 * Apply ready actions at the end of cycle
 * ForceOpen or ForceClose have priority over readyToClose or readyToOpen
 * ForceOpen has also priority over ForceClose
 */
void BlueSeaLatchingRelay::applyReadyActions() {
	if((this->isForceToOpen != 1) && (this->isForceToClose != 1)) {
		if(this->isReadyToOpen == 1) {
			this->setOpened();
		} else if(this->isReadyToClose == 1){
			this->setClosed();
		}
	}
}

void BlueSeaLatchingRelay::startCycle() {
	this->isReadyToOpen = 0;
	this->isReadyToClose = 0;
	this->isForceToOpen = 0;
	this->isForceToClose = 0;
}

/*
* Getting relay state
* comparaison with the logic pin and the analogic state from BlueSea
* !! Relay value > 100 if relay open
*/
byte BlueSeaLatchingRelay::getState() {
	
	if(this->statePin) {
	 	int analogState = analogRead(this->statePin);
	 	if(analogState >= 100 && (this->state != BlueSeaLatchingRelay::RELAY_OPEN)) {
	 		this->state = BlueSeaLatchingRelay::RELAY_OPEN;
	 	}
	 	
	 	if(analogState < 100 && (this->state != BlueSeaLatchingRelay::RELAY_CLOSE)) {
	 		this->state = BlueSeaLatchingRelay::RELAY_CLOSE;
	 	}
	}
	
    return this->state;
}

