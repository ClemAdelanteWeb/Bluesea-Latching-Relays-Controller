#include <AltSoftSerial.h>
#include <Thread.h>;
#include "BlueSeaLatchingRelay.h"
#include <Adafruit_ADS1015.h>
 
//------
// SETTINGS

const byte activatePrintStatus = 1;

// Checking cells differences
// var boolean
const byte activateCheckingCellsVoltageDifference = 1;

// Opening charge relay for SOC >= SOCMax
const int SOCMax = 1000;

// Re-Close Charge Relay when SOCMaxReset is reached
const int SOCMaxReset = 950;

// Opening Load relay if SOC <= SOCmin
const int SOCMin = 350;

// Re-Close Load Relay when SOCMaxReset is reached
const int SOCMinReset = 370;

// SOC Maximum time considerated valid 
// in mS
const int SOCMaxTimeValid = 10000;

// Maximum Voltage security
const int BatteryVoltageMax = 13800;  // 13,8v = 3,45v / Cell

// Waiting for Max Reset Voltage after reaching Max Voltage (time to discharge the battery enough to use it)
const int BatteryVoltageMaxReset = 13360;  

// Minimum Voltage security
const int BatteryVoltageMin = 12800; // 12,8v = 3,2v / Cell

// Waiting for Min Reset Voltage after reaching Min Voltage (time to re-charge the battery enough to use it)
const int BatteryVoltageMinReset = 12900; // 12,9  = 3,225v / Cell

// Minimum operating cell voltage
const int CellVoltageMin = 280;

// Maximum operating cell voltage
const int CellVoltageMax = 380;

// Voltage difference between cells  or batteries
// Absolute value (-100mV) = 100mV).
// in mV
const int CellsDifferenceMaxLimit = 150;

// Voltage difference maximum to considere that the battery bank can be starting using again
// in mV
const int CellsDifferenceMaxReset = 100; 


const byte LoadRelayClosePin = A2;
const byte LoadRelayOpenPin = A3;
const byte LoadRelayStatePin = A7;

const byte ChargeRelayClosePin = A0;
const byte ChargeRelayOpenPin = A1;
const byte ChargeRelayStatePin = A6;

const byte BuzzerPin = 7;

// if pin = 1, BMV infos are collected
const byte ActivateBmvSerialPin = 4;

// Number of cells in the battery
const int unsigned cellsNumber = 4;

// ADS1115 Calibration at 10v *1000
// Ex: 10 / 18107 = 0,000552273
// cell 1, 2, 3, 4 etc
float adc_calibration[] = {
  0.5518956,
  0.54827077,
  0.55232238,
  0.55189998
};

// END SETTINGS
//-------

// Program declarations

// RX pin 8 Tx pin 9
AltSoftSerial Bmv; 

// SoftwareSerial Bmv(BmvRxPin,BmvTxPin); // RX, TX
Thread RunApplication = Thread();

// ADS1115 on I2C0x48 adress
Adafruit_ADS1115 ads(0x48);

// Relays declaration
BlueSeaLatchingRelay LoadRelay;
BlueSeaLatchingRelay ChargeRelay;

unsigned int BatteryVoltage;
unsigned long BatteryVoltageUpdatedTime;

int CellsDifferenceMax;
unsigned long CellsDifferenceMaxUpdatedTime;

unsigned int SOC;
unsigned int SOCTemp;
unsigned int SOCCurrent;
unsigned long SOCUpdatedTime;

// If SOCDisSOCChargeCycling = true : the battery is full and Charge Relay is open 
bool SOCDisSOCChargeCycling = false;

// if SOCChargeCycling = true : the battery is empty and Load relay is opened
bool SOCChargeCycling = false;

// If High voltage has been detected (waiting for HighVoltageReset)
// Charge relay is closed
bool HighVoltageDetected = false;

// If Low voltage has been detected (waiting for LowVoltageReset)
// Load relay is closed
bool LowVoltageDetected = false;

// If a High voltage difference has been detected between cells
// waiting for a lower difference 
// Charge And Load relay are closed, Alarm is ON
bool CellsDifferenceDetected = false;

// If an individual cell voltage is too low
// waiting for a higher value
// force charging, alarm is ON
bool CellVoltageMinDetected = false;

// If an individual cell voltage is too high
// waiting for a lower value
// force discharging, alarm is ON
bool CellVoltageMaxDetected = false;


// victron checksum 
byte checksum = 0;
String V_buffer;
char c;

String MessageTemp;

int isfirstrun = 1;

void log(String message, byte buzz, byte buzzperiode = 100);


void setup()
{
  pinMode(LoadRelayClosePin, OUTPUT);
  pinMode(LoadRelayOpenPin, OUTPUT);
  pinMode(ChargeRelayClosePin, OUTPUT);
  pinMode(ChargeRelayOpenPin, OUTPUT);

  pinMode(ChargeRelayStatePin, INPUT_PULLUP);
  pinMode(LoadRelayStatePin, INPUT_PULLUP);

  pinMode(ActivateBmvSerialPin, INPUT);
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
  RunApplication.setInterval(10000); // 10 sec

  ads.begin();

}
 
void loop() {
  if(isfirstrun) {
    bip(50);
    delay(50);
    bip(50);
    isfirstrun = 0;
  }
  
  if(isEnabledBMVSerialInfos()) {
    readBmvData();
  }

  if(RunApplication.shouldRun()) {
    RunApplication.run();
  }
  
}


/**
 * Checking Voltage AND SOC and close or open relays
 */
void run() {
  ChargeRelay.startCycle();
  LoadRelay.startCycle();

  
  // storing BatteryVoltage in temp variable
  int CurrentBatteryVoltage = getBatteryVoltage();

  // Get actual SOC
  SOCCurrent = getBatterySOC();

  
  // --- Normal LOAD routines
  // checking if Load relay should be closed
  
  // first general condition
  // Normal operating range voltage and relay open
  if((CurrentBatteryVoltage > BatteryVoltageMin) && (LoadRelay.getState() != LoadRelay.RELAY_CLOSE)) {
    
      // not in special event
      if((SOCChargeCycling == false)  && (LowVoltageDetected == false)) {

          // if using BMV SOC
          if(isUseBMVSerialInfos()){
            SOCCurrent = getBatterySOC();
            
            if((SOCCurrent > SOCMin)) {
              LoadRelay.setReadyToClose();        
              log(F("Load r. closing, routine"), 0);   
            }
            
          } 
          // Without SOC
          else {
            LoadRelay.setReadyToClose();        
            log(F("Load r. closing, routine"), 0);   
          }          
      }   
  }  


  // ---- Normal CHARGE routines
  // checking if Charge relay must be closed

  // first general condition
  if((CurrentBatteryVoltage < BatteryVoltageMax) && (ChargeRelay.getState() != ChargeRelay.RELAY_CLOSE)) {
    
      // not in special event
      if((SOCDisSOCChargeCycling == false) && (HighVoltageDetected == false)) {

          // if using BMV SOC
          if(isUseBMVSerialInfos()){
            SOCCurrent = getBatterySOC();
            
            if((SOCCurrent < SOCMax)) {
              ChargeRelay.setReadyToClose();        
              log(F("Charge r. closing, routine"), 0);   
            }
            
          } 
          // Without SOC
          else {
            ChargeRelay.setReadyToClose();        
            log(F("Charge r. closing, routine"), 0);   
          }          
      }   
  }
  

   
  //---
  // Cancelling Charge Cycling
  // 
  if(SOCChargeCycling == true) {
    
    if(isUseBMVSerialInfos()) {
           
        // if SOC > SOCMinReset
        if(SOCCurrent >= SOCMinReset) {
          SOCChargeCycling = false;
          LoadRelay.setReadyToClose();  

          MessageTemp = F("SOC min reset reached : current/min : ");
          MessageTemp += (String)SOCCurrent+" % /"+(String)SOCMinReset;       
          log(MessageTemp, 0);   
        }      
              
      } else {
          // Case IF Charge Cycling = true while it shouldn't
          // could append after disconnecting SOC check and SOCChargeCycling was ON.            
          // if Voltage battery high enough, we close the Load Relay
          
           if(LowVoltageDetected == false) {
            SOCChargeCycling = false;
            LoadRelay.setReadyToClose();
            log(F("Load r. closing, routine without SOC"), 0);   
          } 
      }
  }



  //---
  // Cancelling DisCharge Cycling
  // 
  if(SOCDisSOCChargeCycling == true) {
      if(isUseBMVSerialInfos()) {      
          // if SOC < SOCMaxReset
          if(SOCCurrent <= SOCMaxReset) {
            SOCDisSOCChargeCycling = false;
            ChargeRelay.setReadyToClose();  

            MessageTemp = F("SOC max reset reached : current/max : ");
            MessageTemp += (String)SOCCurrent+" % /"+(String)SOCMaxReset;       
            log(MessageTemp, 0);   
          }   
                   
      } else {
          // Case IF SOCDisSOCChargeCycling = true while it shouldn't
          // could append after disconnecting SOC check and SOCDisSOCChargeCycling was ON. 
          // if Voltage battery Low enough, we close the Charge Relay
          if(HighVoltageDetected == false) {
              SOCDisSOCChargeCycling = false;
              ChargeRelay.setReadyToClose();
              log(F("Charge r. closing, routine without SOC"), 0);   
           }     
      }    
  }
   
   
 // END NORMAL ROUTINES
 // -------------------------

  // if Charge relay has been manualy closed and doesn't match with the code
  // Relay must be opened
 if((SOCDisSOCChargeCycling == true) || (HighVoltageDetected  == true)) {
    if(ChargeRelay.getState() != ChargeRelay.RELAY_OPEN) {
      ChargeRelay.setReadyToOpen();
      log(F("Charge relay state doesn't match, relay opening"), 0);   
    }
 }

  // if Load relay has been manualy closed
  // Relay must be opened
  if((SOCChargeCycling == true) || (LowVoltageDetected  == true)) {
    if(LoadRelay.getState() != LoadRelay.RELAY_OPEN) {
      LoadRelay.setReadyToOpen();
      log(F("Load relay state doesn't match, relay opening"), 0);   
    }
 }



  // STARTING EXCEPTIONAL EVENTS
  if(isUseBMVSerialInfos()) {
  
      // SOC Max detection
      if((SOCCurrent >= SOCMax) && (SOCDisSOCChargeCycling == false)) {
        
        // Open Charge Relay
        SOCDisSOCChargeCycling = true;    
        ChargeRelay.setReadyToOpen();
        
        MessageTemp = F("SOC max reached : ");
        MessageTemp += (String) (SOCCurrent/10.0);
        MessageTemp += F(" %");  
        log(MessageTemp, 0);     
        
      }
  
  
       // SOC Min detection
      if((SOCCurrent <= SOCMin) && (SOCChargeCycling == false)) {
        
        // Open Load Relay
        SOCChargeCycling = true;
    
        LoadRelay.setReadyToOpen();

        MessageTemp = F("SOC min reached : current/min : ");
        MessageTemp += (String)SOCCurrent+" % / "+(String)SOCMin;   
        log(MessageTemp, 0);         
    } 
  } 
  


  //---
  // Voltage verifications
  if((millis() - BatteryVoltageUpdatedTime) < 6000) {

    // high voltage detection
     if((CurrentBatteryVoltage >= BatteryVoltageMax) && (HighVoltageDetected == false)) {
        // Open Charge Relay
        HighVoltageDetected = true;
        ChargeRelay.forceToOpen();
        MessageTemp = F("High v. detected : ");
        MessageTemp += (String) (CurrentBatteryVoltage/1000.0)+" V";
        log(MessageTemp, 0);
    } else {
      
      if(HighVoltageDetected == true) {

        if(ChargeRelay.getState() == ChargeRelay.RELAY_CLOSE) {       
          ChargeRelay.forceToOpen();  
          log(F("High Voltage Detected, force open"), 0);   
        }

        // if Voltage battery low enough, we close the Charge Relay
        if(CurrentBatteryVoltage <= BatteryVoltageMaxReset) {          
          HighVoltageDetected = false;
          ChargeRelay.setReadyToClose();  
          log(F("Voltage Max Reset reached, HighVoltageDetected > false"), 0);   
        }       
      }      
    }

    // Low voltage detection
    if((CurrentBatteryVoltage <= BatteryVoltageMin)  && (LowVoltageDetected == false)) {
       // Open LoadRelay
        LowVoltageDetected = true;
        LoadRelay.forceToOpen();
        log(F("Low Voltage Detected"), 0);   

        // Constrain battery to charge
        // in case of SOC >= max SOC, charge relay is open (can happen when very low consumption is not detected by Victron monitor)
        if(ChargeRelay.getState() == ChargeRelay.RELAY_OPEN) { 
            ChargeRelay.forceToClose();
            MessageTemp = F("Constrain Charge Relay to close. SOC : ");
            MessageTemp += (String) getBatterySOC();
            MessageTemp += F(" . Current voltage : "); 
            MessageTemp += (String) CurrentBatteryVoltage;
            log(MessageTemp, 0); 
        }

      
    } else {
      
      if(LowVoltageDetected == true) {

         if(LoadRelay.getState() == LoadRelay.RELAY_CLOSE) {       
          LoadRelay.forceToOpen();  
          log(F("Low Voltage Detected, force open"), 0);   
        }

         // Constrain battery to charge
        if(ChargeRelay.getState() == ChargeRelay.RELAY_OPEN) { 
            ChargeRelay.forceToClose();
            MessageTemp = F("Constrain Charge Relay to close. secound atempt.");
            log(MessageTemp, 0); 
        }

        
        // if Voltage battery high enough, we close the Load Relay
        if(CurrentBatteryVoltage >= BatteryVoltageMinReset) {
          LowVoltageDetected = false;
          LoadRelay.setReadyToClose();
          log(F("Voltage Min Reset reached, LowVoltageDetected > false"), 0);   
        }       
      }      
    }
    
  } else {
    MessageTemp = F("V. upd > 6s (");
    MessageTemp += (String) (BatteryVoltageUpdatedTime/1000);
    MessageTemp += F(" ms)");   
    log(MessageTemp, 1, 3000);

    LoadRelay.forceToOpen();
    log(F("No Voltage Detected, force open load relay"), 0);   
  }



  //---
  // Checking Cells / Batteries differences
  if(activateCheckingCellsVoltageDifference) {  
    CellsDifferenceMax = getMaxCellVoltageDifference();

    if((millis() - CellsDifferenceMaxUpdatedTime) < 10000) {
      
      // Too much voltage difference detected
       if((CellsDifferenceMax >= CellsDifferenceMaxLimit) && (CellsDifferenceDetected == false)) {
          // Open Load relay
          CellsDifferenceDetected = true;
          LoadRelay.forceToOpen();

          MessageTemp = F("Cells v. diff too high ");
          MessageTemp += (String) CellsDifferenceMax;
          MessageTemp += F(" Mv)");   
          log(MessageTemp, 1, 5000);
      } else {
        
        if(CellsDifferenceDetected == true) {
  
          // if Votage battery low enough, we close the LoadRelay
          if(CellsDifferenceMax <= CellsDifferenceMaxReset) {          
            CellsDifferenceDetected = false;
            LoadRelay.setReadyToClose();  
            
            log(F("Reset OK : Cells v. diff < Reset"), 0);
          }       
          
        }      
      }    
    } else {
      log(F("Cells diff upd > 10s"), 1, 2500);
    }

  }

// #########
   // checking for Individual cell voltage detection
   int i;
    int cellVoltage;
    for (i = (cellsNumber-1); i >= 0; i--) {
        if(i>0) {
           cellVoltage = getAdsCellVoltage(i) - getAdsCellVoltage((i-1));
        } else {
          cellVoltage = getAdsCellVoltage(i);
        }

        // HIGH individual voltage cell detected
        if((cellVoltage > CellVoltageMax) && (CellVoltageMaxDetected == false)) {
           // Open Charge Relay 
          CellVoltageMaxDetected = true;
          ChargeRelay.forceToOpen();
          
          // force discharging
          LoadRelay.forceToClose();
    
          MessageTemp = F("High individual voltage detected on cell ");
          MessageTemp += (String) (i);
          MessageTemp += F(" . Voltage : )");
          MessageTemp += (String) cellVoltage;  
          log(MessageTemp, 1, 2000);
          
        } else {    
          
          if(CellVoltageMaxDetected == true) {
      
            // if voltage cell low enough, we close the LoadRelay
            if(cellVoltage <= CellVoltageMax) {          
              CellVoltageMaxDetected = false; 
              log(F("Reset OK : high voltage cell low enough"), 0);
            }                   
          }      
        }


          // LOW individual voltage cell detected
        if((cellVoltage < CellVoltageMin) && (CellVoltageMinDetected == false)) {
           // Open load Relay, Close charge relay 
          CellVoltageMinDetected = true;
          LoadRelay.forceToOpen();

          // force charging
          ChargeRelay.forceToClose();
    
          MessageTemp = F("Low individual voltage detected on cell ");
          MessageTemp += (String) (i);
          MessageTemp += F(" . Voltage : )");
          MessageTemp += (String) cellVoltage;  
          log(MessageTemp, 1, 2000);
          
        } else {    
          
          if(CellVoltageMinDetected == true) {
      
            // if voltage cell high enough
            if(cellVoltage >= CellVoltageMin) {          
              CellVoltageMinDetected = false;        
              log(F("Reset OK : low voltage cell high enough"), 0);
            }          
          }      
        }  
    }

   

  // Serial.print("Cells voltage max difference : \t");
  // Serial.println(CellsDifferenceMax);
  // Serial.println('--');

  // applying actions on relays
  LoadRelay.applyReadyActions();
  ChargeRelay.applyReadyActions();
  
  if(activatePrintStatus) {
    printStatus();
  }
  // 
 // checkVoltages();
}



/**
 * Get Cell voltage from ADS1115
 * 
 */
int getAdsCellVoltage(unsigned int cellNumber) {

    int16_t adc;
    
    // waiting for correct values
    int unsigned attempts = 0;
    do {
      adc = ads.readADC_SingleEnded((uint8_t) cellNumber);
 
      if(attempts > 0) {
        Serial.println("Attemp : "+(String)+attempts+" / "+(String)+attempts);
      }
      
      attempts++;

      if(attempts >= 50) {
          MessageTemp = F("Cell ");
          MessageTemp += (String) cellNumber;
          MessageTemp += F(" attempts to get voltage > 50)");   
          log(MessageTemp, 1, 3000);
      }
      
    } while(adc <= 0 && attempts <= 50);

 // Serial.println("Cell "+(String)cellNumber+" : "+(String)adc+" : v "+(String) (adc * adc_calibration[cellNumber]));
 
  return adc * adc_calibration[cellNumber];
}

/**
 * Calculate max cell difference between all cells
 * Return value in mV
 */
float getMaxCellVoltageDifference() {
    float maxDiff = 0;

    // Cells voltages
    float cellsVoltage[(cellsNumber-1)];

    int i;
    int cellVoltage;
    for (i = (cellsNumber-1); i >= 0; i--) {
        if(i>0) {
           cellVoltage = getAdsCellVoltage(i) - getAdsCellVoltage((i-1));
        } else {
          cellVoltage = getAdsCellVoltage(i);
        }
       
        cellsVoltage[i] = cellVoltage;
    }

    CellsDifferenceMaxUpdatedTime = millis();

    return getDiffBtwMaxMin(cellsVoltage, (cellsNumber-1));
}


/**
 * Get Battery voltage
 * try with ADS1115 First, Victron BMV next
 */
int getBatteryVoltage() {
  int BatteryVoltageTemp = getAdsBatteryVoltage();
  
  if(BatteryVoltageTemp) {
    return BatteryVoltageTemp;
  }

  return 0;
}

/**
 * Return Battery Voltage (12v) via ADS 
 */
int getAdsBatteryVoltage() {
  int BatteryVoltageTemp = getAdsCellVoltage((cellsNumber-1));

  if(BatteryVoltageTemp > 100) {
      BatteryVoltageUpdatedTime = millis();
  } else {
    log(F("ADS C3 v. not available"),1, 5000);
  }
  
  return BatteryVoltageTemp;
}


/**
 * Return SOC value
 */
int getBatterySOC() {
  if((millis() - SOCUpdatedTime) < SOCMaxTimeValid) {
      return SOC;  
  } else {
      return 0;
  }
}


// Reading Victron Datas
// And extracting SOC and Voltage values
void readBmvData() {

  
  if (Bmv.available()) {
    c = Bmv.read();
    checksum += c;

    if (V_buffer.length() < 80) {
      V_buffer += c;
    }
      
    // end of line
    if (c == '\n') { 
    
      // Serial.println(V_buffer);
       if (V_buffer.startsWith("SOC")) {
        String temp_string = V_buffer.substring(V_buffer.indexOf("\t")+1);
        SOCTemp = temp_string.toInt();
       }
    
        // end of serie
      if (V_buffer.startsWith("Checksum")) {       
        
          byte result = checksum % 256;
    
          // checksum OK
         if(result == 0) {          
           SOC = SOCTemp;
           SOCUpdatedTime = millis();
         } else {
           // Checksum error
         }
    
          // begin new serie
         checksum = 0;
       }
    
      // begin new line
      V_buffer="";
    }     
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


/**
 * Return the value between Max and min values in an array
 */
float getDiffBtwMaxMin(float *values, int sizeOfArray) {
  float maxValue=0; 
  float minValue=0;
  float diffValue;
  
  for (byte k=0; k < sizeOfArray; k++) {
    if (values[k] > maxValue) {
       maxValue = values[k];  
    }
  }
  
  for (byte k=0; k < sizeOfArray; k++){
    if ((values[k] < minValue) || (minValue == 0)) {
       minValue = values[k];
    }
  }

  diffValue = (maxValue-minValue);
  
  return diffValue;
}

// Check if SOC Value is valid
boolean isSOCValid() {
   if((millis() - SOCUpdatedTime) < SOCMaxTimeValid) {
      return true;
   }

   return false;  
}

void log(String message, byte buzz, byte buzzperiode = 100) {
  // TODO
  // trigger alarm and led... 

  if(buzz) {
    bip(buzzperiode);
    delay(buzzperiode);
    bip(buzzperiode);
    delay(buzzperiode);
    bip(buzzperiode);
    delay(buzzperiode);
  
    Serial.print(F("ALARME : "));
  } else {
    Serial.print(F("ALERTE : "));
  }
 
  Serial.println(message);
  Serial.println(F("----"));
}


void printStatus() {
  Serial.println();
  //Serial.println(" ------- Status --- ");
   Serial.print(F("Detected v. : Low / High  ")); Serial.print(LowVoltageDetected);Serial.print(F(" / ")); Serial.println(HighVoltageDetected);
  Serial.print(F("V. : ")); Serial.println(getBatteryVoltage());
  Serial.print(F("V. Max / Rst : ")); Serial.print(BatteryVoltageMax);  Serial.print(F(" / ")); Serial.println(BatteryVoltageMaxReset);
  Serial.print(F("V. Min / Rst : ")); Serial.print(BatteryVoltageMin);Serial.print(F(" / ")); Serial.println(BatteryVoltageMinReset);
  Serial.print(F("Charge r. status : ")); Serial.println(ChargeRelay.getState());
  Serial.print(F("Load r. status : "));  Serial.println(LoadRelay.getState());
  Serial.print(F("SOC Enable / Used / value: ")); Serial.print(isEnabledBMVSerialInfos()); Serial.print(F(" / ")); Serial.print(isUseBMVSerialInfos()); Serial.print(F(" / ")); Serial.println(getBatterySOC());  
  Serial.print(F("SOC Max/ Max Rst : ")); Serial.print(SOCMax); Serial.print(F(" / ")); Serial.println(SOCMaxReset);    
  Serial.print(F("SOC Min/ Min Rst : ")); Serial.print(SOCMin); Serial.print(F(" / ")); Serial.println(SOCMinReset);  

  Serial.print(F("Cycling : Discharge / Charge ")); Serial.print(SOCDisSOCChargeCycling);Serial.print(F(" / ")); Serial.println(SOCChargeCycling);

  // Cells Voltage
  int i;
  int cellVoltage;
  for (i = (cellsNumber-1); i >= 0; i--) {
      if(i>0) {
         cellVoltage = getAdsCellVoltage(i) - getAdsCellVoltage((i-1));
      } else {
        cellVoltage = getAdsCellVoltage(i);
      }
     
    Serial.print(F("Cell : "));
    Serial.print(i);
    Serial.print(F(" / "));
      
    Serial.print((cellVoltage/1000.0),3);
    Serial.print(F(" :  "));
    Serial.println(getAdsCellVoltage(i));
  }

  Serial.print(F("Cells Diff/Max/Rst : "));  Serial.print(CellsDifferenceMax);  Serial.print(F(" mV / ")); Serial.print(CellsDifferenceMaxLimit);  Serial.print(F(" / ")); Serial.println(CellsDifferenceMaxReset);
  Serial.println();
    
}

/**
 * Detection if BMV Serial should be collected and used
 */
boolean isEnabledBMVSerialInfos() {

   // If PIn activated
   if(digitalRead(ActivateBmvSerialPin) == HIGH) {
      return true;
  }
    
  return false;  
}

/**
 * Check if BMV Infos can be used
 */
boolean isUseBMVSerialInfos() {

   // If PIn activated
   if(isEnabledBMVSerialInfos()) {

      // AND SOC valid
      if(isSOCValid()) {
        return true;
      } 
  }
    
  return false;  
}
