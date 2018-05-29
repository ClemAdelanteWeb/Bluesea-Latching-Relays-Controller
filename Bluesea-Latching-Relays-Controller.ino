#include <SoftwareSerial.h>;
#include <Thread.h>;
#include <BlueSeaLatchingRelay.h>
#include <Adafruit_ADS1015.h>
 
//------
// SETTINGS

const bool activatePrintStatus = true;

// Read BMV Serial And checking SOC value
// var boolean
const byte activateBMVSerialInfos = 1;

// Checking cells differences
// var boolean
const byte activateCheckingCellsVoltageDifference = 1;

// Opening charge relay for SOC > ChargeMax
const int SOCMax = 932;

// Closing charge relay for SOC < ChargeMin 
const int SOCMaxReset = 930;

// Opening Load relay for SOC < LoadMin
const int SOCMin = 300;

// Closing charge relay for SOC < LoadMin 
const int SOCMinReset = 350;

const int BatteryVoltageMax = 13600;
const int BatteryVoltageMaxReset = 13550; 

const int BatteryVoltageMin = 12400;
const int BatteryVoltageMinReset = 12800;

// Voltage difference between cells  or batteries
// Absolute value (-100mV) = 100mV).
// in mV
const int CellsDifferenceMaxLimit = 100;

// Voltage difference maximum to considere that the battery bank can be starting using again
// in mV
const int CellsDifferenceMaxReset = 25; 


const byte LoadRelayClosePin = A2;
const byte LoadRelayOpenPin = A3;
const byte LoadRelayStatePin = A6;

const byte ChargeRelayClosePin = A0;
const byte ChargeRelayOpenPin = A1;
const byte ChargeRelayStatePin = A7;

const byte BmvRxPin = 2;
const byte BmvTxPin = 3;

const byte BuzzerPin = 7;


// ADS1115 Calibration at 10v
// Ex: 10 / 18107 = 0,000552273
const float adc0_calibration = 0.552029;
const float adc1_calibration = 0.548878;
const float adc2_calibration = 0.552486;
const float adc3_calibration = 0.552486;

// END SETTINGS
//-------

// Program declarations
SoftwareSerial Bmv(BmvRxPin,BmvTxPin); // RX, TX
Thread RunApplication = Thread();

// ADS1115 on I2C0x48 adress
Adafruit_ADS1115 ads(0x48);
float Voltage = 0.0;
int16_t adc0, adc1, adc2, adc3;


BlueSeaLatchingRelay LoadRelay;
BlueSeaLatchingRelay ChargeRelay;

const byte buffsize = 32; 
const char EOPmarker = '\n'; //This is the end of packet marker
char serialbuf[buffsize]; //This gives the incoming serial some room. Change it if you want a
char tempChars[buffsize];  

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

// victron
bool BeginBmvDataSerie = false;

// victron checksum 
const byte num_keywords = 13;
const byte value_bytes = 33;
const byte label_bytes = 9;
bool blockend = false;
static byte blockindex = 0;
char recv_label[num_keywords][label_bytes]  = {0};  // {0} tells the compiler to initalize it with 0. 
char recv_value[num_keywords][value_bytes]  = {0};  // That does not mean it is filled with 0's
unsigned SOCLastValue;

String MessageTemp;

void log(String message, byte buzz, byte buzzperiode = 100);

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

  if(activateBMVSerialInfos) {
    Bmv.begin(19200); 
  }

  RunApplication.onRun(run);
  RunApplication.setInterval(5000);

  ads.begin();

}
 
void loop() {
  if(activateBMVSerialInfos) {
    readBmvData();
  }

  if(RunApplication.shouldRun()) {
    RunApplication.run();
  }
  
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



int getBatterySOC() {
  return SOC;  
}

/**
 * Checking Voltage AND SOC and close or open relays
 */
void run() {
  ChargeRelay.startCycle();
  LoadRelay.startCycle();

  
  // storing BatteryVoltage in temp variable
  int CurrentBatteryVoltage = getBatteryVoltage();

  
  // --- Normal LOAD routines
  // checking if Load relay should be closed
  
  // first general condition
  // Normal operating range voltage and relay open
  if((CurrentBatteryVoltage > BatteryVoltageMin) && (LoadRelay.getState() != LoadRelay.RELAY_CLOSE)) {
    
      // not in special event
      if(ChargeCycling == false) {

          // if using BMV SOC
          if(activateBMVSerialInfos){
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
      if(DischargeCycling == false) {

          // if using BMV SOC
          if(activateBMVSerialInfos){
            SOCCurrent = getBatterySOC();
            
            if((SOCCurrent < SOCMax)) {
              ChargeRelay.setReadyToClose();        
              log(F("Charge r. closing, routine"), 0);   
            }
            
          } 
          // Without SOC
          else {
            ChargeRelay.setReadyToClose();        
            log(F("Load r. closing, routine"), 0);   
          }          
      }   
  }
  
   // END NORMAL ROUTINES


  // STARTING EXCEPTIONAL EVENTS
  if(activateBMVSerialInfos) {
    SOCCurrent = getBatterySOC();
    
    //---
    // SOC verifications
    if((millis() - SOCUpdatedTime) < 20000) {
  
      // SOC Max detection
      if((SOCCurrent >= SOCMax) && (DischargeCycling == false)) {
        
        // Open Charge Relay
        DischargeCycling = true;    
        ChargeRelay.setReadyToOpen();
        
        MessageTemp = F("SOC max acheived : ");
        MessageTemp += (String) (SOCCurrent/10.0);
        MessageTemp += F(" %)");  
        log(MessageTemp, 0);     
        
      } else {
        
        if(DischargeCycling == true) {
  
          // if SOC low enough, we can close the Charge Relay
          if(SOCCurrent <= SOCMaxReset) { 
                     
            DischargeCycling = false;
            //if(ChargeRelay.getState() != ChargeRelay.RELAY_CLOSE) {
              
              ChargeRelay.setReadyToClose();  
              
              MessageTemp = F("SOC max reset acheived : ");
              MessageTemp += (String) (SOCCurrent/10.0);
              MessageTemp += F(" %)");  
              log(MessageTemp, 0);      
           // }
            
                   
          }       
        }  else {
           // Waiting for discharging battery (cycling) in order to acheive SOCMaxReset
        }
      }
  
  
       // SOC Min detection
      if((SOCCurrent <= SOCMin) && (ChargeCycling == false)) {
        
        // Open Load Relay
        ChargeCycling = true;
    
        LoadRelay.setReadyToOpen();

        
        MessageTemp = F("SOC min acheived : current/min : ");
        MessageTemp += (String)SOCCurrent+" % /"+(String)SOCMin;   
        log(MessageTemp, 0);     
      } else {
        if(ChargeCycling == true) {
  
          // if SOC High enough, we close the Charge Relay
          if(SOCCurrent >= SOCMinReset) {          
            ChargeCycling = false;
            LoadRelay.setReadyToClose();  

            MessageTemp = F("SOC min reset acheived : current/min : ");
            MessageTemp += (String)SOCCurrent+" % /"+(String)SOCMinReset;       
            log(MessageTemp, 0);   
          }       
        }       
      }
      
    } else {      
        MessageTemp = F("SOC upd time > 20s ");
        MessageTemp += (String) (SOCUpdatedTime/1000)+" s";
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
        }

        // if Voltage battery low enough, we close the Charge Relay
        if(CurrentBatteryVoltage <= BatteryVoltageMaxReset) {          
          HighVoltageDetected = false;
          ChargeRelay.setReadyToClose();  
        }       
      }      
    }

    // Low voltage detection
    if((CurrentBatteryVoltage <= BatteryVoltageMin)  && (LowVoltageDetected == false)) {
       // Open LoadRelay
        LowVoltageDetected = true;
        LoadRelay.forceToOpen();
      
    } else {
      
      if(LowVoltageDetected == true) {

        // if Voltage battery high enough, we close the Load Relay
        if(CurrentBatteryVoltage >= BatteryVoltageMinReset) {
          LowVoltageDetected = false;
          LoadRelay.setReadyToClose();
        }       
      }      
    }
    
  } else {
    MessageTemp = F("V. upd > 6s (");
    MessageTemp += (String) (BatteryVoltageUpdatedTime/1000);
    MessageTemp += F(" ms)");   
    log(MessageTemp, 1, 3000);
  }


  // ---- Checking status relays and Cyclings concordances ---
  // Discharge Status
  if(DischargeCycling) {
    // incorrect, ChargeRelay should be Open, because Cycling routine
    if(ChargeRelay.getState() == ChargeRelay.RELAY_CLOSE) {      
        // canceling cycling
        DischargeCycling = 0;
        log(F("Canceling DischargeCycling"), 0);
    }
  }

  // Charging status
  if(ChargeCycling) {
    // incorrect, LoadRelay should be Open, because Cycling routine
    if(LoadRelay.getState() == LoadRelay.RELAY_CLOSE) {      
        // canceling cycling
        ChargeCycling = 0;
        log(F("Canceling ChargeCycling"), 0);
    }
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
            
            log(F("Cells v. diff < Reset"), 0);
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
  int16_t adc = ads.readADC_SingleEnded(cellNumber);

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

    float adc3Voltage = getAdsCellVoltage(3);
    float adc2Voltage = getAdsCellVoltage(2);
    float adc1Voltage = getAdsCellVoltage(1);
    float adc0Voltage = getAdsCellVoltage(0);

    cellsVoltage[3] = adc3Voltage-adc2Voltage;
    cellsVoltage[2] = adc2Voltage-adc1Voltage;
    cellsVoltage[1] = adc1Voltage-adc0Voltage;
    cellsVoltage[0] = adc0Voltage;

    CellsDifferenceMaxUpdatedTime = millis();

    return getDiffBtwMaxMin(cellsVoltage, 4);
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
        // Serial.print(ptr);

            // SOC search
            if (strcmp(ptr, "SOC") == 0) {
              socLine = true;
            } else {
              if(socLine == true) {
                  SOCTemp = atoi(ptr);   
                  Serial.println("SOC : "+(String)SOCTemp);
              }
            }

            // BMV Checksum verification
            if (strcmp(ptr, "Checksum") == 0) {
 
              // We got a whole block into the received data.
              // Check if the data received is not corrupted.
              // Sum off all received bytes should be 0;
              byte checksum = 0;
              for (int x = 0; x < blockindex; x++) {
                  // Loop over the labels and value gotten and add them.
                  // Using a byte so the the % 256 is integrated. 
                  char *v = recv_value[x];
                  char *l = recv_label[x];
                  while (*v) {
                      checksum += *v;
                      v++;
                  }
                  while (*l) {
                      checksum+= *l;
                      l++;
                  }
                  // Because we strip the new line(10), the carriage return(13) and 
                  // the horizontal tab(9) we add them here again.  
                  checksum += 32;
              }

              // Serial.println((String) checksum+" : Checksum ");
              
              // Checksum should be 0, so if !0 we have correct data.
              if (!checksum) {    
                  if(SOCTemp) {

                    // Other protection, if SOC difference between current value and last received value > 2
                    // we considere that the current value is not valid.
                    int tempValue = SOCTemp - SOCLastValue;
                    if(abs(tempValue) <= 2) {                        
                      SOC = SOCTemp;                      
                      SOCUpdatedTime = millis();
                    }          

                     SOCLastValue = SOCTemp;
                  }

                  
              } else {
                  MessageTemp = F("Bad checksum detected : ");
                  MessageTemp += (String) checksum;
                 log(MessageTemp, 0);
                 //Serial.println(checksum);    
              }
              
              // Reset the block index, and make sure we clear blockend.
              blockindex = 0;
              SOCTemp = 0;
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
 * Search Cells Voltages and Battery voltage
 */
void checkVoltages() {
  adc0 = ads.readADC_SingleEnded(0);
  Voltage = (adc0 * adc0_calibration)/1000;

   Serial.print("AIN0: "); 
  Serial.print(adc0);
  Serial.print("\t Voltage: ");
  Serial.println(Voltage, 7); 

     adc1 = ads.readADC_SingleEnded(1);
  Voltage = (adc1 * adc1_calibration)/1000;

   Serial.print("AIN1: "); 
  Serial.print(adc1);
  Serial.print("\t Voltage: ");
  Serial.println(Voltage, 7);  

    adc2 = ads.readADC_SingleEnded(2);
  Voltage = (adc2 * adc2_calibration)/1000;

   Serial.print("AIN2: "); 
  Serial.print(adc2);
  Serial.print("\t Voltage: ");
  Serial.println(Voltage, 7);  

  adc3 = ads.readADC_SingleEnded(3);
  Voltage = (adc3 * adc3_calibration)/1000;

  Serial.print("AIN3: "); 
  Serial.print(adc3);
  Serial.print("\t Voltage: ");
  Serial.println(Voltage, 7);  
  Serial.println("--");  
  
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
   Serial.print(F("Detected v. : Low / High  ")); Serial.print(HighVoltageDetected);Serial.print(F(" / ")); Serial.println(LowVoltageDetected);
  Serial.print(F("V. : ")); Serial.println(getBatteryVoltage());
  Serial.print(F("V. Max / Rst : ")); Serial.print(BatteryVoltageMax);  Serial.print(F(" / ")); Serial.println(BatteryVoltageMaxReset);
  Serial.print(F("V. Min / Rst : ")); Serial.print(BatteryVoltageMin);Serial.print(F(" / ")); Serial.println(BatteryVoltageMinReset);
  Serial.print(F("Charge r. status : ")); Serial.println(ChargeRelay.getState());
  Serial.print(F("Load r. status : "));  Serial.println(LoadRelay.getState());
  Serial.print(F("SOC : "));  Serial.println(getBatterySOC());  
  Serial.print(F("SOC Max/ Max Rst : ")); Serial.print(SOCMax); Serial.print(F(" / ")); Serial.println(SOCMaxReset);    
  Serial.print(F("SOC Min/ Min Rst : ")); Serial.print(SOCMin); Serial.print(F(" / ")); Serial.println(SOCMinReset);  

  Serial.print(F("Cycling : Discharge / Charge ")); Serial.print(DischargeCycling);Serial.print(F(" / ")); Serial.println(ChargeCycling);

  // Cells Voltage
  float voltageLastCell = 0;
  float voltageTotalCell = 0;
  voltageTotalCell = getAdsCellVoltage(0);
  Voltage = voltageTotalCell;
  Serial.print(F("Cell 0 : "));
  Serial.println((Voltage/1000.00));

  voltageLastCell = voltageTotalCell;
  voltageTotalCell = getAdsCellVoltage(1);
  Voltage = voltageTotalCell-voltageLastCell;
  Serial.print(F("Cell 1 : "));
  Serial.println((Voltage/1000.00));

  voltageLastCell = voltageTotalCell;
  voltageTotalCell = getAdsCellVoltage(2);
  Voltage = voltageTotalCell-voltageLastCell;
  Serial.print(F("Cell 2 : "));
  Serial.println((Voltage/1000.00));

  voltageLastCell = voltageTotalCell;
  voltageTotalCell = getAdsCellVoltage(3);
  Voltage = voltageTotalCell-voltageLastCell;
  Serial.print(F("Cell 3 : "));
  Serial.println((Voltage/1000.00));
  Serial.print(F("Cells Diff/Max/Rst : "));  Serial.print(getMaxCellVoltageDifference());  Serial.print(F(" mV / ")); Serial.print(CellsDifferenceMaxLimit);  Serial.print(F(" / ")); Serial.println(CellsDifferenceMaxReset);
  
  Serial.println();
    
}

