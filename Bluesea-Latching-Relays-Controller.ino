#include <SoftwareSerial.h>;
#include <Thread.h>;
#include <Relay.h>
 
//------
// SETTINGS

// Opening charge relay for SOC > ChargeMax
const int SOCChargeMax = 998;

// Closing charge relay for SOC < ChargeMin 
const int SOCChargeReset = 997;

// Opening Load relay for SOC < LoadMin
const int SOCLoadMin = 300;

// Closing charge relay for SOC < LoadMin 
const int SOCLoadMinReset = 960;

const int BatteryVoltageMax = 13900;
const int BatteryVoltageMaxReset = 12990; //BatteryVoltageMax - 1000

const int BatteryVoltageMin = 12400;
const int BatteryVoltageMinReset = 12990; // BatteryVoltageMin + 1000

// Voltage difference between cells  or batteries
// Absolute value (-100mV^) = 100mV).
// in mV
const int CellsDifferenceMax = 100;

// Voltage difference maximum to considere that the battery bank can be starting using again
// in mV
const int CellsDifferenceMaxReset = 50; // CellsDifferenceMax - 50


const byte LoadRelayClosePin = A4;
const byte LoadRelayOpenPin = A5;
const byte LoadRelayStatePin = A7;

const byte ChargeRelayClosePin = A2;
const byte ChargeRelayOpenPin = A3;
const byte ChargeRelayStatePin = A6;

const byte BmvRxPin = 2;
const byte BmvTxPin = 3;

const byte BuzzerPin = 12;

// END SETTINGS
//-------

// Program declarations
SoftwareSerial Bmv(BmvRxPin,BmvTxPin); // RX, TX
Thread RunApplication = Thread();

Relay LoadRelay;
Relay ChargeRelay;

const byte buffsize = 32; 
const char EOPmarker = '\n'; //This is the end of packet marker
char serialbuf[buffsize]; //This gives the incoming serial some room. Change it if you want a
char tempChars[buffsize];  

unsigned int BatteryVoltage;
unsigned long BatteryVoltageUpdatedTime;

int CellsDifference;
unsigned long CellsDifferenceUpdatedTime;

unsigned int SOC;
unsigned long SOCUpdatedTime;

// relays
byte DischargeCycling = 2;
byte LoadRelayStatus = 2;
byte ChargeRelayStatus = 2;

byte HighVoltageDetected = 2;
byte LowVoltageDetected = 2;
byte CellsDifferenceDetected = 2;

// victron
bool BeginBmvDataSerie = false;

void setup()
{
  pinMode(LoadRelayClosePin, OUTPUT);
  pinMode(LoadRelayOpenPin, OUTPUT);
  pinMode(ChargeRelayClosePin, OUTPUT);
  pinMode(ChargeRelayOpenPin, OUTPUT);

  pinMode(ChargeRelayStatePin, INPUT_PULLUP);
  pinMode(LoadRelayStatePin, INPUT_PULLUP);

  pinMode(BuzzerPin, OUTPUT);
  
  // Load Relay declaration
  LoadRelay.name = "Load relay";
  LoadRelay.openPin = LoadRelayOpenPin;
  LoadRelay.closePin = LoadRelayClosePin;
  LoadRelay.statePin = LoadRelayStatePin;

  // Charge Relay declaration
  ChargeRelay.name = "Charge relay";
  ChargeRelay.openPin = ChargeRelayOpenPin;
  ChargeRelay.closePin = ChargeRelayClosePin;
  ChargeRelay.statePin = ChargeRelayStatePin;
  
  Serial.begin(19200);
  Bmv.begin(19200); 

  RunApplication.onRun(run);
  RunApplication.setInterval(1000);

}
 
void loop() {

  readBmvData();

  if(RunApplication.shouldRun()) {
    RunApplication.run();
  }
  
}

void getBatteryVoltage() {
  return BatteryVoltage;
}

void getBatterySOC() {
  return SOC;  
}

/**
 * Checking Voltage AND SOC and close or open relays
 */
void run() {

  
  //---
  // SOC verifications

  if((millis() - SOCUpdatedTime) < 6000) {
    if(SOC >= SOCChargeMax) {
       // Open Charge Relay
       DischargeCycling = true;
  
      ChargeRelay.setOpened();
        Serial.print("SOC max atteint : ");
        Serial.println(SOC);
    }

    // Closing Charge Relay to charge Batteries
    if(SOC <= SOCChargeReset) {
      DischargeCycling = false;
      LoadRelay.setClosed();
    }
    
  } else {
    // TODO
      log("SOC updated time > 6s", 1);
  }

  Serial.print("SOC : ");
  Serial.println(SOC);
  Serial.println('--');

  //---
  // Voltage verifications
  if((millis() - BatteryVoltageUpdatedTime) < 6000) {

    // high voltage detection
     if((BatteryVoltage >= BatteryVoltageMax) && (HighVoltageDetected == false)) {
        // Open LoadRelay
        HighVoltageDetected = true;
        ChargeRelay.setOpened();
        Serial.print("High voltage detected : ");
        Serial.println(BatteryVoltage);
    } else {
      
      if(HighVoltageDetected == true) {

        // if Votage battery low enough, we close the LoadRelay
        if(BatteryVoltage <= BatteryVoltageMaxReset) {          
          HighVoltageDetected == false;
          ChargeRelay.setClosed();  
        }       
        
      }      
    }

    // Low voltage detection
    if((BatteryVoltage <= BatteryVoltageMin)  && (LowVoltageDetected == false)) {
       // Open LoadRelay
        LowVoltageDetected = true;
        LoadRelay.setOpened();
      
    } else {
      
      if(LowVoltageDetected == true) {

        // if Votage battery high enough, we close the LoadRelay
        if(BatteryVoltage >= BatteryVoltageMinReset) {
          LowVoltageDetected == false;
          LoadRelay.setClosed();
        }       
      }      
    }
    
  } else {
    // TODO
    log("Battery voltage updated > 6s", 0);
  }

  Serial.print("Voltage : ");
  Serial.println(BatteryVoltage);
  Serial.println('--');



  //---
  // Checking Cells / Batteries differences
  if((millis() - CellsDifferenceUpdatedTime) < 10000) {

    // high voltage detection
     if((CellsDifference >= CellsDifferenceMax) && (CellsDifferenceDetected == false)) {
        // Open Load AND charge Relay 
        CellsDifferenceDetected = true;
        ChargeRelay.setOpened();
        LoadRelay.setOpened();
    } else {
      
      if(CellsDifferenceDetected == true) {

        // if Votage battery low enough, we close the LoadRelay
        if(CellsDifference <= CellsDifferenceMaxReset) {          
          CellsDifferenceDetected == false;
          ChargeRelay.setClosed();  
          LoadRelay.setClosed();  
        }       
        
      }      
    }    
  } else {
    // TODO
   log("Cells voltage difference updated > 10s", 0);
  }

  Serial.print("Cells voltage differrence : ");
  Serial.println(CellsDifference);
  Serial.println('--');
  
}



void log(String message, byte buzz) {
  // TODO
  // trigger alarm and led... 

  if(buzz) {
    bip(100);
    delay(100);
    bip(100);
    delay(100);
    bip(100);
    delay(100);
  
    Serial.print("ALARME : ");
  } else {
    Serial.print("ALERTE : ");
  }
 
  Serial.println(message);
}

// Reading Victron Datas
// And extracting SOC and Voltage values
void readBmvData() {
  char splitLabelValueDelimiter[] = "\t";

  while (Bmv.available() > 0) { 
    
      static int bufpos = 0; 
      char inchar = Bmv.read();
      
      //Serial.write(inchar);
       
      if (inchar != EOPmarker) { 
        serialbuf[bufpos] = inchar;
        bufpos++; 
      }
      else { 
        
      //Serial.println(serialbuf);
        strcpy(tempChars, serialbuf);
        char* ptr = strtok(tempChars, splitLabelValueDelimiter);

        bool voltageLine = false;
        bool socLine = false;
        while(ptr != NULL) {

          // voltage search
          if (strcmp(ptr, "V") == 0) {
            voltageLine = true;
          } else {
            if(voltageLine == true) {
                BatteryVoltage = atoi(ptr);
                BatteryVoltageUpdatedTime = millis();

                // Serial.println(BatteryVoltage);
            }
          }

          // SOC search
          if (strcmp(ptr, "SOC") == 0) {
            socLine = true;
          } else {
            if(socLine == true) {
                SOC = atoi(ptr);
                SOCUpdatedTime = millis();              
            }
          }

            // create next part
            ptr = strtok(NULL, splitLabelValueDelimiter);
        }
        
         serialbuf[bufpos] = 0; //restart the buff
         bufpos = 0; //restart the position of the buff
      }
    
      yield();
  }
}

/**
 * Trigger Buzzer
 */
void bip(int duration) {
    digitalWrite(BuzzerPin, HIGH); 
    delay(duration);    
    digitalWrite(BuzzerPin, LOW); 
}

