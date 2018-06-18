#include <AltSoftSerial.h>
#include <Thread.h>;
#include <BlueSeaLatchingRelay.h>
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
const int BatteryVoltageMaxReset = 13400;  // 13,4v = 3,35v / Cell

// Minimum Voltage security
const int BatteryVoltageMin = 12800; // 12,8v = 3,2v / Cell

// Waiting for Min Reset Voltage after reaching Min Voltage (time to re-charge the battery enough to use it)
const int BatteryVoltageMinReset = 12900; // 12,8  = 3,225v / Cell

// Voltage difference between cells  or batteries
// Absolute value (-100mV) = 100mV).
// in mV
const int CellsDifferenceMaxLimit = 80;

// Voltage difference maximum to considere that the battery bank can be starting using again
// in mV
const int CellsDifferenceMaxReset = 40; 


const byte LoadRelayClosePin = A2;
const byte LoadRelayOpenPin = A3;
const byte LoadRelayStatePin = A7;

const byte ChargeRelayClosePin = A0;
const byte ChargeRelayOpenPin = A1;
const byte ChargeRelayStatePin = A6;

const byte BuzzerPin = 7;

// if pin = 1, BMV infos are collected
const byte ActivateBmvSerialPin = 4;


// ADS1115 Calibration at 10v *1000
// Ex: 10 / 18107 = 0,000552273
const float adc0_calibration = 0.55240650;
const float adc1_calibration = 0.54914015;
const float adc2_calibration = 0.55267991;
const float adc3_calibration = 0.55247466;

// END SETTINGS
//-------

// Program declarations

// RX pin 8 Tx pin 9
AltSoftSerial Bmv; 

// SoftwareSerial Bmv(BmvRxPin,BmvTxPin); // RX, TX
Thread RunApplication = Thread();

// ADS1115 on I2C0x48 adress
Adafruit_ADS1115 ads(0x48);
float Voltage = 0.0;
int16_t adc0, adc1, adc2, adc3;


BlueSeaLatchingRelay LoadRelay;
BlueSeaLatchingRelay ChargeRelay;

unsigned int BatteryVoltage;
unsigned long BatteryVoltageUpdatedTime;

unsigned int Cell0Voltage;
unsigned int Cell1Voltage;
unsigned int Cell2Voltage;
unsigned int Cell3Voltage;

int CellsDifferenceMax;
unsigned long CellsDifferenceMaxUpdatedTime;

unsigned int SOC;
unsigned int SOCTemp;
unsigned int SOCCurrent;
unsigned long SOCUpdatedTime;

// If DischargeCycling = true : the battery is full and Charge Relay is open 
bool DischargeCycling = false;

// if ChargeCycling = true : the battery is empty and Load relay is opened
bool ChargeCycling = false;

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
    bip(3000);
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
      if((ChargeCycling == false)  && (LowVoltageDetected == false)) {

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
      if((DischargeCycling == false) && (HighVoltageDetected == false)) {

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
  if(ChargeCycling == true) {
    
    if(isUseBMVSerialInfos()) {
           
        // if SOC > SOCMinReset
        if(SOCCurrent >= SOCMinReset) {
          ChargeCycling = false;
          LoadRelay.setReadyToClose();  

          MessageTemp = F("SOC min reset reached : current/min : ");
          MessageTemp += (String)SOCCurrent+" % /"+(String)SOCMinReset;       
          log(MessageTemp, 0);   
        }      
              
      } else {
          // Case IF Charge Cycling = true while it shouldn't
          // could append after disconnecting SOC check and ChargeCycling was ON.            
          // if Voltage battery high enough, we close the Load Relay
          if(CurrentBatteryVoltage >= BatteryVoltageMinReset) {
            ChargeCycling = false;
            LoadRelay.setReadyToClose();
            log(F("Load r. closing, routine without SOC"), 0);   
          } 
      }
  }



  //---
  // Cancelling DisCharge Cycling
  // 
  if(DischargeCycling == true) {
      if(isUseBMVSerialInfos()) {      
          // if SOC < SOCMaxReset
          if(SOCCurrent <= SOCMaxReset) {
            DischargeCycling = false;
            ChargeRelay.setReadyToClose();  

            MessageTemp = F("SOC max reset reached : current/max : ");
            MessageTemp += (String)SOCCurrent+" % /"+(String)SOCMaxReset;       
            log(MessageTemp, 0);   
          }   
                   
      } else {
          // Case IF DischargeCycling = true while it shouldn't
          // could append after disconnecting SOC check and DischargeCycling was ON. 
          // if Voltage battery Low enough, we close the Charge Relay
          if(CurrentBatteryVoltage <= BatteryVoltageMaxReset) {
              DischargeCycling = false;
              ChargeRelay.setReadyToClose();
              log(F("Charge r. closing, routine without SOC"), 0);   
           }     
      }    
  }
   
   
 // END NORMAL ROUTINES
 // -------------------------

  // if Charge relay has been manualy closed and doesn't match with the code
 if((DischargeCycling == true) || (HighVoltageDetected  == true)) {
    if(ChargeRelay.getState() != ChargeRelay.RELAY_OPEN) {
      ChargeRelay.setReadyToOpen();
      log(F("Charge relay state doesn't match, relay opening"), 0);   
    }
 }

  // if Load relay has been manualy closed
  if((ChargeCycling == true) || (LowVoltageDetected  == true)) {
    if(LoadRelay.getState() != LoadRelay.RELAY_OPEN) {
      LoadRelay.setReadyToOpen();
      log(F("Load relay state doesn't match, relay opening"), 0);   
    }
 }



  // STARTING EXCEPTIONAL EVENTS
  if(isUseBMVSerialInfos()) {
  
      // SOC Max detection
      if((SOCCurrent >= SOCMax) && (DischargeCycling == false)) {
        
        // Open Charge Relay
        DischargeCycling = true;    
        ChargeRelay.setReadyToOpen();
        
        MessageTemp = F("SOC max reached : ");
        MessageTemp += (String) (SOCCurrent/10.0);
        MessageTemp += F(" %");  
        log(MessageTemp, 0);     
        
      }
  
  
       // SOC Min detection
      if((SOCCurrent <= SOCMin) && (ChargeCycling == false)) {
        
        // Open Load Relay
        ChargeCycling = true;
    
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

        if(ChargeRelay.getState() == 1) {       
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
      
    } else {
      
      if(LowVoltageDetected == true) {

         if(LoadRelay.getState() == 1) {       
          LoadRelay.forceToOpen();  
          log(F("Low Voltage Detected, force open"), 0);   
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
  }



  //---
  // Checking Cells / Batteries differences
  if(activateCheckingCellsVoltageDifference) {  
    CellsDifferenceMax = getMaxCellVoltageDifference();

    if((millis() - CellsDifferenceMaxUpdatedTime) < 10000) {
      
      // high voltage detection
       if((CellsDifferenceMax >= CellsDifferenceMaxLimit) && (CellsDifferenceDetected == false)) {
          // Open Load AND charge Relay 
          CellsDifferenceDetected = true;
          ChargeRelay.forceToOpen();
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
            ChargeRelay.setReadyToClose();  
            LoadRelay.setReadyToClose();  
            
            log(F("Reset OK : Cells v. diff < Reset"), 0);
          }       
          
        }      
      }    
    } else {
      log(F("Cells diff upd > 10s"), 1, 2500);
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
int getAdsCellVoltage(int cellNumber) {

    int16_t adc;
    
    // waiting for correct values
    int unsigned attempts = 0;
    do {
      adc = ads.readADC_SingleEnded(cellNumber);
 
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

// Serial.println("Cell "+(String)cellNumber+" : "+(String)adc);

  if(cellNumber == 0) {
    return adc * adc0_calibration;
  } else if(cellNumber == 1) {
    return adc * adc1_calibration;
  } else if(cellNumber == 2) {
    return adc * adc2_calibration;
  } else if(cellNumber == 3) {
    return adc * adc3_calibration;
  }

  return 0;
}

/**
 * Calculate max cell difference between all cells
 * Return value in mV
 */
float getMaxCellVoltageDifference() {
    float maxDiff = 0;

    // Cells voltages
    float cellsVoltage[] = {0,0,0,0};

    Cell3Voltage = getAdsCellVoltage(3);
    Cell2Voltage = getAdsCellVoltage(2);
    Cell1Voltage = getAdsCellVoltage(1);
    Cell0Voltage = getAdsCellVoltage(0);

    cellsVoltage[3] = Cell3Voltage-Cell2Voltage;
    cellsVoltage[2] = Cell2Voltage-Cell1Voltage;
    cellsVoltage[1] = Cell1Voltage-Cell0Voltage;
    cellsVoltage[0] = Cell0Voltage;

    CellsDifferenceMaxUpdatedTime = millis();

    return getDiffBtwMaxMin(cellsVoltage, 4);
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
  int BatteryVoltageTemp = getAdsCellVoltage(3);

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

  Serial.print(F("Cycling : Discharge / Charge ")); Serial.print(DischargeCycling);Serial.print(F(" / ")); Serial.println(ChargeCycling);

  // Cells Voltage
  float voltageLastCell = 0;
  float voltageTotalCell = 0;
  voltageTotalCell = Cell0Voltage;
  Voltage = voltageTotalCell;
  Serial.print(F("Cell 0 : "));
  Serial.print((Voltage/1000.0),3);
    Serial.print(F(" :  "));
    Serial.println(voltageTotalCell);

  voltageLastCell = voltageTotalCell;
  voltageTotalCell = Cell1Voltage;
  Voltage = voltageTotalCell-voltageLastCell;
  Serial.print(F("Cell 1 : "));
  Serial.print((Voltage/1000.0),3);
    Serial.print(F(" :  "));
    Serial.println(voltageTotalCell);

  voltageLastCell = voltageTotalCell;
  voltageTotalCell = Cell2Voltage;
  Voltage = voltageTotalCell-voltageLastCell;
  Serial.print(F("Cell 2 : "));
  Serial.print((Voltage/1000.0),3);
    Serial.print(F(" :  "));
    Serial.println(voltageTotalCell);

  voltageLastCell = voltageTotalCell;
  voltageTotalCell = Cell3Voltage;
  Voltage = voltageTotalCell-voltageLastCell;
  Serial.print(F("Cell 3 : "));
  Serial.print((Voltage/1000.0),3);
    Serial.print(F(" :  "));
    Serial.println(voltageTotalCell);
    
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


