/**
 * This example turns the ESP32 into a Bluetooth LE keyboard that writes the words, presses Enter, presses a media key and then Ctrl+Alt+Delete
 */
#include <BleKeyboard.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>
#include <EEPROM.h>

BleKeyboard bleKeyboard;
Adafruit_ADS1115 ads;  /* Use this for the 16-bit version */

float cellVoltage = 0.0;
//float ADC2V = 5454.5455; //ADC value / ADC2V = Volts
float ADC2V = 4013.436;
const char vers[] = "1.0-02282020"; 

//32 chars with /r/n + 8 corrections (e.g. 4 backspace + 4 chars)
#define MAXCHARS 40
char receivedChars[MAXCHARS];
boolean newData = false;
unsigned short curVoltage = 0, lastVoltage = 0;

float cellVoltageAvg = 0;
float cellVoltageArray[8];
unsigned char arrayIdx = 0;
unsigned char settle = 0;
unsigned char done = 0;

void setup() {
  unsigned short calValue = 0;
  
  Serial.begin(115200);
  Serial.println("Starting BLE work!");
  bleKeyboard.begin();
  EEPROM.begin(2); //Use 2 bytes in EEPROM for calibration

  calValue = EEPROM.read(0);
  calValue += ((EEPROM.read(1))<<8);
  if(calValue > 12000 || calValue < 8000)
  {
    Serial.println("> Vcal out of range, using default.");
  }
  else
  {
    Serial.print("> Vcal value: ");
    Serial.println(calValue);
    ADC2V = ADC2V * (calValue/10000.0);
  }

  ads.begin();
  ads.setGain(GAIN_ONE);
  ads.setSPS(ADS1115_DR_32SPS);
}

void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarkers[] = {'t', 'n', '?', 'v', 's', 'z'};
  char endMarkers[] = {'\n', '\r'};
  char rc;

  while (Serial.available() > 0 && newData == false) {
    rc = Serial.read();
    if (rc == '\b')
      Serial.print("\b \b");
    else
      Serial.print(rc);
        
    if (recvInProgress == true) {
      //Serial.println("Receive in progress");
      if (rc == endMarkers[0] || rc == endMarkers[1]){
        //Serial.println("Received endMarker");
        receivedChars[ndx] = '\0'; // terminate the string
        /*Serial.print("ndx=");
          Serial.println(ndx);
          Serial.print("rc=");
          Serial.print(rc);*/
        recvInProgress = false;
        ndx = 0;
        newData = true;
      } 
      else if (rc == '\b') { //Backspace = do not increment character index, do not write new character
        /*Serial.println("Received BS");
        Serial.print("ndx=");
        Serial.println(ndx);
        Serial.print("rc=");
        Serial.print(rc);*/
        if (ndx == 0) {
          ndx = 0;
        }
        else {
          //receivedChars[ndx] = 0;
          ndx--;
        }
      }
      else {
        /*Serial.println("Received normal");
          Serial.print("ndx=");
          Serial.println(ndx);
          Serial.print("rc=");
          Serial.print(rc);*/
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= MAXCHARS) {
          ndx = MAXCHARS - 1;
        }
      }
    }

    else if (rc == startMarkers[0] || rc == startMarkers[1] || rc == startMarkers[2] || rc == startMarkers[3] || rc == startMarkers[4] || rc == startMarkers[5]) {
      /*Serial.println("Received startMarker");
        Serial.print("ndx=");
        Serial.println(ndx);
        Serial.print("rc=");
        Serial.print(rc);*/
      recvInProgress = true;
      receivedChars[ndx] = rc;
      ndx++;
      if (ndx >= MAXCHARS) {
        ndx = MAXCHARS - 1;
      }
    }

    else if (rc == endMarkers[0] || rc == endMarkers[1]) {
      Serial.print("\r\n");
      Serial.print("> ");
    }
  }
}

int fast_atoi_leading_pos( const char * p )
{
    int x = 0;
    ++p;
    while (*p >= '0' && *p <= '9') {
        x = (x*10) + (*p - '0');
        ++p;
    }
    return x;
}

int fast_atoi_leading_neg( const char * p )
{
    int x = 0;
    bool neg = false;
    ++p;
    if (*p == '-') {
        neg = true;
        ++p;
    }
    while (*p >= '0' && *p <= '9') {
        x = (x*10) + (*p - '0');
        ++p;
    }
    if (neg) {
        x = -x;
    }
    return x;
}

void printMenu()
{
    //'t', 'n', '?', 'v', 's', 'z'
    Serial.print("\r\n");
    Serial.println("> Select Mode:");
    Serial.println(">  Cal R/W Mode");
    Serial.println(">   t[r/w] a[address: 0-999] d[data, unsigned int (0-65535)]");
    Serial.println(">  Help (Prints this menu)");
    Serial.println(">   ?");
    Serial.println(">  Stop current mode/test");
    Serial.println(">   n[1-2]");
    Serial.println(">  Version");
    Serial.println(">   v");
    Serial.println(">  Soft Reset");
    Serial.println(">   z");
    Serial.println(">  Status");
    Serial.println(">   s");
    Serial.print("\r\n");
    Serial.print("> ");
}

void loop() {
  unsigned char i = 0;
  unsigned short wAddress = 0, wData = 0;
  unsigned char intPart = 0;
  unsigned short decPart = 0;
  char *args[8];
  recvWithStartEndMarkers();
  
  if (newData == true)
  {
    args[i] = strtok(receivedChars, " ");
    while (args[i] != NULL) //i = number of arguments in received string
    {
      args[++i] = strtok(NULL, " ");
    }
    switch (args[0][0]) {
      case 't':
        if(args[0][1] == 'w')
        {
          Serial.println("\r\n> Writing EEPROM 16b: ");
          //aXXX = address in dec.
          wAddress = fast_atoi_leading_pos(args[1]);
          //dXXX = data in dec.
          wData = fast_atoi_leading_pos(args[2]);
          Serial.print("> Address 0x");
          Serial.print(wAddress, HEX);
          Serial.print(", Data = dec. ");
          Serial.println(wData);
          EEPROM.write(wAddress, (wData&0xFF));
          EEPROM.write((wAddress+1), ((wData&0xFF00)>>8));
          EEPROM.commit();
          Serial.println("> Write finished");
        }
        else if(args[0][1] == 'r')
        {
          Serial.println("\r\n> Reading EEPROM 8b: ");
          wAddress = fast_atoi_leading_pos(args[1]);
          Serial.print("> Address 0x");
          Serial.print(wAddress, HEX);
          wData = EEPROM.read(wAddress);
          Serial.print(", Data = dec. ");
          Serial.println(wData);
          Serial.println("> Read finished");
        }
        break;
      case 'n':
        break;
      case 'v':        
        Serial.print("\r\n> Software version: ");
        Serial.println(vers);
        Serial.print("\r\n> ");
        break;
      case 's':
        break;
      case '?':
        printMenu();
        //Timer2.pause(); //Start the timer counting
        break;
      case 'z':
        //Soft reset
        ESP.restart();
        break;
      default:
        printMenu();
        break;
    }

    newData = false;
  }
  
  // 1x gain   0-6.0075V 1 bit = 183.33uV
  //Serial.print("ADC raw: ");
  //Serial.println(ads.readADC_SingleEnded(0));
  cellVoltage = (float)ads.readADC_SingleEnded(0)/ADC2V;
  cellVoltageArray[arrayIdx] = cellVoltage;
  arrayIdx++;
  if(arrayIdx > 7)
    arrayIdx = 0;
  cellVoltageAvg = 0;
  for(i = 0; i < 8; i++)
  {
    cellVoltageAvg += cellVoltageArray[i];
  }
  cellVoltageAvg = cellVoltageAvg / 8.0;
  curVoltage = (int)(cellVoltage*100);
  intPart = (int)cellVoltageAvg;
  decPart = (int)(cellVoltageAvg*10000) - intPart*10000;
  
  Serial.print(intPart);
  Serial.print(".");
  if(decPart < 10)
  {
    Serial.print("000");
  }
  else if(decPart < 100)
  {
    Serial.print("00");
  }
  else if(decPart < 1000)
  {
    Serial.print("0");
  }
  Serial.print(decPart);
  Serial.println("V");
  
  if(bleKeyboard.isConnected()) {
    if(curVoltage > 20) //Current voltage >200mV
    {
      //Serial.print("gt20, ");
      if(curVoltage != lastVoltage)
      {
        //Serial.print("cv!=lv, ");
        settle = 0;
        lastVoltage = curVoltage;
        done = 0;
      }
      if(settle > 8 && done != 1)
      {
        done = 1;
        //Serial.println("s>8");
        /*Serial.print("CV ");
        Serial.print(curVoltage);
        Serial.print("LV ");
        Serial.println(lastVoltage);*/
        bleKeyboard.print(intPart);
        bleKeyboard.print(".");
        if(decPart < 10)
        {
          bleKeyboard.print("000");
        }
        else if(decPart < 100)
        {
          bleKeyboard.print("00");
        }
        else if(decPart < 1000)
        {
          bleKeyboard.print("0");
        }
        bleKeyboard.print(decPart);
        bleKeyboard.write(KEY_RETURN);
        Serial.print("Threshold met, ");
        Serial.print(intPart);
        Serial.print(".");
        if(decPart < 10)
        {
          Serial.print("000");
        }
        else if(decPart < 100)
        {
          Serial.print("00");
        }
        else if(decPart < 1000)
        {
          Serial.print("0");
        }
        Serial.print(decPart);
        Serial.println("V");
  
        delay(1000);
      }
      //Serial.println("s+");
      settle++;
      if(settle>255)
        settle == 255;
    }
  }
  
  delay(100);
}

