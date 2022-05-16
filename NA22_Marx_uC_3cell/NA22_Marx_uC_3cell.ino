// Version 0: Initial release, 5/2/2022 M. Kemp


#include <SPI.h>
#include <string.h>
#define MAX_STRING_LEN  406    //used for serial recieve

//Pin definitions
#define CS_1 2
#define Charge_disable 3
#define Trigger_disable 4
#define Coil1_enable 5
#define Coil2_enable 6
#define Battery_uC_enable 7
#define Stat1 8
#define Stat2 9

//program global variables
byte    Stat1_value = 0;
byte    Stat2_value = 0;
int     HV_condition_value = 0;
int     Bus_condition_value = 0;
int     TS3_value = 0;
int     HV_desired = 1023;    //default to lowest output voltage, 2^10-1 counts
byte    Charge_disable_value = 1;
byte    Trigger_disable_value = 1;
byte    Coil1_enable_value = 0;
byte    Coil2_enable_value = 0;
byte    Battery_uC_enable_value = 0;

String content = "";          //used for serial recieve
const byte numChars = 406;    //used for serial recieve
char receivedBytes[numChars]; //used for serial recieve
boolean newData = false;      //used for serial recieve
char *record1;                //used for serial recieve
char *p, *i;                  //used for serial recieve

//Set up the speed, data order and data mode
//SettingsB is AD5270, <50 MHz, MSB first, sample on falling clock
SPISettings settingsB(5000000, MSBFIRST, SPI_MODE1); 

void setup() {
  Serial.begin(38400);        // Initialize serial communication at 38400 bits per second
  pinMode(CS_1, OUTPUT);      //CS1 for U2, rheostat for Vout control
  pinMode(Charge_disable, OUTPUT);
  digitalWrite(Charge_disable,HIGH);      //Set to disable
  pinMode(Trigger_disable, OUTPUT);
  digitalWrite(Trigger_disable,HIGH);     //Set to disable
  pinMode(Coil1_enable, OUTPUT);
  digitalWrite(Coil1_enable,LOW);         //Set to disable
  pinMode(Coil2_enable, OUTPUT);
  digitalWrite(Coil2_enable,LOW);         //Set to disable
  pinMode(Battery_uC_enable, OUTPUT);
  digitalWrite(Battery_uC_enable,LOW);    //Set to disable
  pinMode(Stat1, INPUT);
  pinMode(Stat2, INPUT);
  digitalWrite(CS_1,HIGH);                //Set to disable
  digitalWrite(CS_1,LOW);                //Set to disable
  digitalWrite(CS_1,HIGH);                //Set to disable
  SPI.begin();                            //Initialize SPI
  delay(1000);

  /////////
  //Set output voltage to minimum
  //enable rheostat
  byte high = B00011100;  //command 7 0x1C
  byte lo = B00000010;    // 0x02
  SPI.beginTransaction(settingsB);
  digitalWrite (CS_1, LOW);
  SPI.transfer(high); 
  SPI.transfer(lo); 
  digitalWrite (CS_1, HIGH);
  SPI.endTransaction();
  //////////////////

  // "Command 1"
  //update rheostat
  //control bits: B000001 (6bits)
  //followed by 10 bits
  int new_Vdes = 1023; //10bits
  lo = new_Vdes & B11111111; //low 8 bits
  high = B00000100 | ((new_Vdes>>8)&B11); //control bits plus high 2 bits
  SPI.beginTransaction(settingsB);
  digitalWrite (CS_1, LOW);
  SPI.transfer(high); 
  SPI.transfer(lo);         
  digitalWrite (CS_1, HIGH);
  SPI.endTransaction();
  /////////////////
}

//Main loop
void loop() {
    //Read stat1 and stat2 and set global values
    Stat1_value = digitalRead(Stat1);
    Stat2_value = digitalRead(Stat2);

    //Read analog values
    int    temp = 0;
    for (byte j=0; j<10; j++){
      temp = temp + analogRead(A0);   //This is HV bus
    }
    HV_condition_value = round(temp / 10);

    temp = 0;
    for (byte j=0; j<10; j++){
      temp = temp + analogRead(A1);   //This is 14V bus
    }
    Bus_condition_value = round(temp / 10);

    temp = 0;
    for (byte j=0; j<10; j++){
      temp = temp + analogRead(A2);   //This is TS3 value
    }
    TS3_value = round(temp / 10);

    //update rheostat
    //control bits: B000001 (6bits)
    //followed by 10 bits
    int new_Vdes = HV_desired; //10bits
    byte lo = new_Vdes & B11111111; //low 8 bits
    byte high = B00000100 | ((new_Vdes>>8)&B11); //control bits plus high 2 bits
    SPI.beginTransaction(settingsB);
    digitalWrite (CS_1, LOW);
    SPI.transfer(high); 
    SPI.transfer(lo);         
    digitalWrite (CS_1, HIGH);
    SPI.endTransaction();
    /////////////////

    //Recieve and handle serial inputs
    recvWithStartEndBytes(); //handles serial
    record1 = receivedBytes; //bitches about this. char to *char    
    processNewData();        //parse into variables, check fault, update DACs
}

void processNewData() {
  if (newData == true) {
    //store parced data in variables
    String temp_S;
    byte control;
    byte cell_num;
    char cstr[5];
    
    if(record1[0]=='C'){//if this a control byte
      temp_S = String(subStr(record1, " ", 1+1)); //first byte after C is cell num
      cell_num = temp_S.toInt(); //This is the previous cell number
      temp_S = String(subStr(record1, " ", cell_num+3));
      control = temp_S.toInt(); //loaded the corresponding control byte
      Trigger_disable_value = control & B1;                   //bit 0
      digitalWrite(Trigger_disable,Trigger_disable_value);
      Charge_disable_value = (control>>1) & B1;               //bit 1
      digitalWrite(Charge_disable,Charge_disable_value);
      Coil1_enable_value = (control>>2) & B1;                 //bit 2
      digitalWrite(Coil1_enable,Coil1_enable_value);
      Coil2_enable_value = (control>>3) & B1;                 //bit 3
      digitalWrite(Coil2_enable,Coil2_enable_value);
      Battery_uC_enable_value = (control>>4) & B1;            //bit 4
      digitalWrite(Battery_uC_enable,Battery_uC_enable_value);
      control = control | ((B00000000 | (Stat1_value&B1))<<5);//bit 5
      control = control | ((B00000000 | (Stat2_value&B1))<<6);//bit 6
      //Bit 7 unused for now

      //update control string with the cell_num
      
      cell_num = cell_num + 1;
      sprintf(cstr, "%03d", cell_num);
      record1[2] = cstr[0];
      record1[3] = cstr[1];
      record1[4] = cstr[2];

      //update control string with the updated control byte
      sprintf(cstr, "%03d", control);
      record1[(cell_num)*4+2] = cstr[0];
      record1[(cell_num)*4+2+1] = cstr[1];
      record1[(cell_num)*4+2+2] = cstr[2];

      //send to next cell
      Serial.print("!");
      Serial.print(record1);
      Serial.println(",");     
    }
    else if (record1[0]=='H'){ //HV des and act
      temp_S = String(subStr(record1, " ", 1+1)); //first byte after H is cell num
      cell_num = temp_S.toInt(); //This is the previous cell number
      temp_S = String(subStr(record1, " ", cell_num*1+3)); //One int per cell
      HV_desired = temp_S.toInt(); //loaded the desired HV voltage

      //update read string with the cell_num
      cell_num = cell_num + 1;
      sprintf(cstr, "%03d", cell_num);
      record1[2] = cstr[0];
      record1[3] = cstr[1];
      record1[4] = cstr[2];

      //update control string with the updated control byte
      sprintf(cstr, "%04d", HV_condition_value);
      record1[(cell_num-1)*5+6] = cstr[0];
      record1[(cell_num-1)*5+7] = cstr[1];
      record1[(cell_num-1)*5+8] = cstr[2];
      record1[(cell_num-1)*5+9] = cstr[3];

      //send to next cell
      Serial.print("!");
      Serial.print(record1);
      Serial.println(",");  
    }
    else if (record1[0]=='B'){
      temp_S = String(subStr(record1, " ", 1+1)); //first byte after H is cell num
      cell_num = temp_S.toInt(); //This is the previous cell number
      
      //update read string with the cell_num
      cell_num = cell_num + 1;
      sprintf(cstr, "%03d", cell_num);
      record1[2] = cstr[0];
      record1[3] = cstr[1];
      record1[4] = cstr[2];

      //update control string with the updated control byte
      sprintf(cstr, "%04d", Bus_condition_value);
      record1[(cell_num-1)*5+6] = cstr[0];
      record1[(cell_num-1)*5+7] = cstr[1];
      record1[(cell_num-1)*5+8] = cstr[2];
      record1[(cell_num-1)*5+9] = cstr[3];

      //send to next cell
      Serial.print("!");
      Serial.print(record1);
      Serial.println(","); 
      
    }
    else if (record1[0]=='T'){
      temp_S = String(subStr(record1, " ", 1+1)); //first byte after H is cell num
      cell_num = temp_S.toInt(); //This is the previous cell number
      
      //update read string with the cell_num
      cell_num = cell_num + 1;
      sprintf(cstr, "%03d", cell_num);
      record1[2] = cstr[0];
      record1[3] = cstr[1];
      record1[4] = cstr[2];

      //update control string with the updated control byte
      sprintf(cstr, "%04d", TS3_value);
      record1[(cell_num-1)*5+6] = cstr[0];
      record1[(cell_num-1)*5+7] = cstr[1];
      record1[(cell_num-1)*5+8] = cstr[2];
      record1[(cell_num-1)*5+9] = cstr[3];

      //send to next cell
      Serial.print("!");
      Serial.print(record1);
      Serial.println(","); 
    }

    newData = false;
  }
}

//Serial recieving function
void recvWithStartEndBytes() {
  static boolean recvInProgress = false;
  static byte ndx = 0;                      
  char startByte = '!';                // <- start byte  is a '!'
  char endByte = ',';                  // <- stop byte   is a ','
  char rb;                            
  while (Serial.available() > 0 && newData == false) {
   rb = Serial.read();
   if (recvInProgress == true) {
      if (rb != endByte) {
        receivedBytes[ndx] = rb;
        ndx++;
        if (ndx >= numChars) {
          ndx = numChars - 1;
        }
      }
      else {
        receivedBytes[ndx] = '\0';            // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
      }
    }
    else if (rb == startByte) {
        recvInProgress = true;
    }
  }
}

// Function to return a substring defined by a delimiter at an index
char* subStr (char* str, char *delim, int index) {
  char *act, *sub, *ptr;
  static char copy[MAX_STRING_LEN];
  int i;
  strcpy(copy, str);
  for (i = 1, act = copy; i <= index; i++, act = NULL) {
     sub = strtok_r(act, delim, &ptr);
     if (sub == NULL) break;
  }
  return sub;
}
