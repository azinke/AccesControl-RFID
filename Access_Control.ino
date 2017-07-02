/*
 * @Author 1: AKAKOTO Boris Marius 
 * @Author 2: AMOUSSOU Zinsou Kenneth
 * @Project: ACCESS CONTROL
 * @Summary: 
 * 
 * @University: UAC/EPAC
 * @Department: EE
 * @Date: 30/06/2017
 * @updated at: ~
 * @Version: 1.0
 */

/*
 * GENERAL USE LIBRARY
 */
 #include <MFRC522.h>
 #include <SPI.h>

/*
 * FUNCTION PROTOTYPE
 */
 void readRFID();
 void clearInterrupt(MFRC522 rfid);
 bool isGranted(MFRC522 rfid);
 void activateRec(MFRC522 rfid);

/*
 * PIN DEFINITION
 */
 
/* Analog input */
 #define BATTERY 0

 /* Digital input */
 #define ACCESS_GRANTED 5 // Green led  ** OUTPUT **
 #define ACCESS_DENIED 3 // Red led     ** OUTPUT **
 #define BATTERY_FAULT 4 // Yellow led  ** OUTPUT **
 #define BUZZER 6        //             ** OUTPUT **
 #define DOOR 7          //             ** OUTPUT **    
 #define INHIBITION 8  //               ** INPUT **

/*
 * GENERAL PURPOSE DEFINITION
 */
  #define RST_PIN 9  /* RFID module PIN config - Reset */
  #define SDA_PIN  10 /* RFID module PIN config -  SDA */
  #define RFID_IRQ 2 /* RFID module PIN config -  IRQ */

  #define VOLTAGE_THRESHOLD 10
  
  /* DOOR */
  #define LOCK 1
  #define UNLOCK 0
  #define DOOR_OPEN_MIN_TIME 10000

 /*
  * VARIABLES
  */

  /*
   * RFID MODULE INIT
   */
  MFRC522 rfid(SDA_PIN, RST_PIN);
  MFRC522::MIFARE_Key key;

  /*
   * Global variable
   */
    bool ag = false; /* Access granted */
    bool toogle = false, buzzer = false, v_toogle = false;
    bool door_alert = false;
    bool low_voltage_alert = false;
    int led_clk = 0, v_led_clk = 0, door_clk = 0;
    int receptionEnable_clk = 0;
    int v_acquisition_clk = 0;
    int door_open = 0; /* Count the time since the door opening */
    unsigned char regVal = 0x7F;
    volatile bool intFlag = false; /* Interrupt flag */

    /* Valid code:  59, 37, 249, 101 }, {21, 234, 64, 119 } */
    byte allowedCode[2][4] = {{ 59, 37, 249, 101 }, {21, 234, 64, 119 }}; 
void setup() {

  Serial.begin(9600);
  /*
   * PIN CONFIGURATIONS
   */
   pinMode(ACCESS_GRANTED, OUTPUT);
   pinMode(ACCESS_DENIED, OUTPUT);
   pinMode(BATTERY_FAULT, OUTPUT);
   pinMode(BUZZER, OUTPUT);
   pinMode(DOOR, OUTPUT);

   pinMode(INHIBITION, INPUT_PULLUP); /* Internal Pull up resistor enable */
   pinMode(RFID_IRQ, INPUT_PULLUP);   /* Internal Pull up resistor enable */

   /*
    * INITIAL STATE
    */
    digitalWrite(ACCESS_GRANTED, LOW); digitalWrite(ACCESS_DENIED, HIGH);
    digitalWrite(BATTERY_FAULT, LOW);
    digitalWrite(BUZZER, LOW);  digitalWrite(DOOR, LOCK);

    /*
     * INITIALIZE SPI & RFID MODULE
     */
     SPI.begin();
     rfid.PCD_Init();

     regVal = 0xA0; /* rx irq */
     rfid.PCD_WriteRegister(rfid.ComIEnReg,regVal);
     intFlag = false;

    /*
     * INTERRUPT INITIALISATION
     */
    attachInterrupt(digitalPinToInterrupt(RFID_IRQ), readRFID, FALLING);

    do{ /* clear a spourious interrupt at start */
      ;
    }while(!intFlag);
    intFlag = false;

}

void loop(){
  /*
   * RFID module Branch
   */
  if(intFlag){
    rfid.PICC_ReadCardSerial();
    if(isGranted(rfid)){
      for(byte k=0; k<4; k++){
        Serial.println(rfid.uid.uidByte[k]);
      }
      digitalWrite(DOOR, UNLOCK);
      ag = true;
    } else { digitalWrite(DOOR, LOCK); ag = false; }
    
    clearInterrupt(rfid);
    rfid.PICC_HaltA();
    intFlag = false;
  }

  /*
   * Inhibition enable branch
   */
  if(!digitalRead(INHIBITION)) {
    digitalWrite(DOOR, UNLOCK);
    ag = true;
  }

  /*
   * Blink output led for energy saving
   */
  if(ag){
    if(led_clk>=5){
      toogle ^= 1;
      digitalWrite(ACCESS_GRANTED, toogle);
      digitalWrite(ACCESS_DENIED, LOW);
      led_clk = 0;
    }
  }else{
    if(led_clk>=5){
      toogle ^= 1;
      digitalWrite(ACCESS_GRANTED, LOW);
      digitalWrite(ACCESS_DENIED, toogle);
      led_clk = 0;
    }

    /*
     * DOOR CLOSE / OPEN check
     */
     /*
    if(digitalRead(DOOR_STATE)){
      door_open++;
      if(door_open>=DOOR_OPEN_MIN_TIME){ door_alert = true; door_open = 0; }
      if(door_alert){
        if(door_clk>=500){
          buzzer ^= true;
          digitalWrite(DOOR, buzzer);
          door_clk = 0;
        }
      }
    }else{
      door_alert = false;
      digitalWrite(DOOR, LOW);
    }*/
  }
  
  /*
   * BATTERY LEVEL CHECKING
   * ** Every ten secons **
   */
  if(v_acquisition_clk >= 10000){
    if(analogRead(BATTERY)<= VOLTAGE_THRESHOLD) low_voltage_alert = true;
    else {
      low_voltage_alert = false;
      digitalWrite(BATTERY_FAULT, LOW);
    }
    v_acquisition_clk = 0;
  }
  if(low_voltage_alert){
    if(v_led_clk>=10){
      v_toogle ^= true;
      digitalWrite(BATTERY_FAULT, v_toogle);
      v_led_clk = 0;
    }
  }

  
  if(receptionEnable_clk>=100) { activateRec(rfid); receptionEnable_clk = 0; }

  delay(1); /* 1 ms delay */
  led_clk++;
  door_clk++;
  receptionEnable_clk++;
  v_acquisition_clk++;
  v_led_clk++;
}

/*
 *  Function: activateRec
 *  @parameter: MFRC522 rfid
 *  @return : none
 *  @summary: Tell to the MFRC522 the needed commands 
 *            to activate the reception
 */
void activateRec(MFRC522 rfid){
    rfid.PCD_WriteRegister(rfid.FIFODataReg, rfid.PICC_CMD_REQA);
    rfid.PCD_WriteRegister(rfid.CommandReg, rfid.PCD_Transceive);  
    rfid.PCD_WriteRegister(rfid.BitFramingReg, 0x87);    
}


/*
 *  Function: readRFID
 *  @parameter: none
 *  @return : none
 *  @summary: Interrupt service routine.
 *    Handle RFID module interrupt
 */
void readRFID(){
   intFlag = true;
}


/*
 *  Function: clearInterrupt
 *  @parameter: MFRC522 rfid
 *  @return : none
 *  @summary: Clear interrupt
 */
void clearInterrupt(MFRC522 rfid){
   rfid.PCD_WriteRegister(rfid.ComIrqReg,0x7F);
}

/*
 *  Function: isGranted
 *  @parameter: MFRC522 rfid
 *  @return : bool
 *  @summary: Check tag Uid and test whether it's allowed
 *    or not.
 */
bool isGranted(MFRC522 rfid){
  unsigned int i = 0, j = 0;
  bool access_granted = true;
  for(i = 0; i < 2; i++){
    Serial.println("Boucle n°1");
    for(j = 0; j < rfid.uid.size; j++){
      if(allowedCode[i][j] == rfid.uid.uidByte[j]){
        access_granted &= true;
      } else access_granted = false;
      
    }
    if(access_granted) return true;
  }
  return false;
}

