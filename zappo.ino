// In order to compile make sure arduino/libraries/robot control is NOT there
// because it conflicts with Adafruit_GFX.h
#include <Wire.h>
#include <Bounce.h>
#include <DS1307new.h>
#include <Time.h>
#include "Adafruit_LEDBackpack.h"
#include "Adafruit_GFX.h"

// Set up our pins
#define lightPin 3        // Analog pin 3
#define filePotPin 2      // Analog pin 2
#define busyPin 2         // the pin number of the SOMO busy pin
#define dataPin 3         // the pin number of the SOMO data pin
#define clockPin 4        // the pin number of the SOMO clock pin
#define resetPin 5        // the pin number of the SOMO reset pin
#define shftInLtchPin 6
#define shftInClkPin 7
#define shftInDataPin 8
#define shftOutClkPin 9
#define shftOutLtchPin 10
#define shftOutDataPin 11
#define speakerFETPin 12

/*
  Buttons:
  1 = alrmSwitch
  2 = snzBtn
  3 = hrBtn
  4 = mnBtn
  5 = playBtn
  6 = setBtn
  DIPS:
  1 = snzUnlmt
*/

// 7 Segment display
Adafruit_7segment matrix = Adafruit_7segment();

// SOMO volume
const unsigned int VOLUME_0 = 0xFFF0;
const unsigned int VOLUME_1 = 0xFFF1;
const unsigned int VOLUME_2 = 0xFFF2;
const unsigned int VOLUME_3 = 0xFFF3;
const unsigned int VOLUME_4 = 0xFFF4;
const unsigned int VOLUME_5 = 0xFFF5;
const unsigned int VOLUME_6 = 0xFFF6;
const unsigned int VOLUME_7 = 0xFFF7;
const unsigned int PLAY_PAUSE = 0xFFFE;
const unsigned int STOP = 0xFFFF;

// Alarm Settings
#define snzTime 6    // amount of mins for the initial snooze
#define snzDegrade 1 // amount of mins for to degrade time by
#define snzLmt 4     // number of times to loop before degrading

// File playing pot control
// Adjust for number of sound files
#define filePotMin 12   // Minumum number for the file potentiometer
#define filePotMax 26   // Maximum number for the file potentiometer

// Clock variables
byte lastSecond   = 0;
byte snzCnt       = 0;
byte snzTimeCur   = snzTime; // current length of the snooze
byte alrmOn       = 0;
boolean showColon = 0; // Used to the colon on the clock
boolean isPMVar   = 0;
uint32_t alrmSec; // Holds the nunber of seconds since 2000 from RTC for snoozing
int newHour       = 0;
int newMinute     = 0;

// Shift Out "pins"
boolean mouthOn   = 0;
boolean noiseOn   = 0;
boolean headOn    = 0;
boolean lightOn   = 0;
byte shftOutValue = 0;

// Shift In Data
int alrmFile;
int alrmHour;
int alrmMin;
int disHour;
int disMin;

// Buttons
boolean alrmSwitch = 0;
boolean snzBtn     = 0;
boolean hrBtn      = 0;
boolean mnBtn      = 0;
boolean setBtn     = 0;
boolean playBtn    = 0; // Button to play a sound file
boolean snzUnlmt   = 0; // unlimited switch - dip #1
int hrBtnLast      = 0;
long hrBtnTime     = 0;
int mnBtnLast      = 0;
long mnBtnTime     = 0;
int playBtnLast    = 0;
long playBtnTime   = 0;
int snzBtnLast     = 0;
long snzBtnTime    = 0;

void setup()
{
  Serial.begin(9600);
  Serial.println("Hello World");
  matrix.begin(0x70);
  
  printFullTime();
   
  // Setup all our pins
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  pinMode(resetPin, OUTPUT);
  pinMode(busyPin, INPUT);
  pinMode(shftInLtchPin, OUTPUT);
  pinMode(shftInClkPin, OUTPUT); 
  pinMode(shftInDataPin, INPUT);
  pinMode(shftOutLtchPin, OUTPUT);
  pinMode(shftOutClkPin, OUTPUT);
  pinMode(shftOutDataPin, OUTPUT);
  pinMode(speakerFETPin, OUTPUT);

  // Set up the SOMO pins
  digitalWrite(clockPin, HIGH);
  digitalWrite(dataPin, LOW);

  resetSomo();

  sendCommand(VOLUME_7);
}

// *********************************************
// MAIN (LOOP)
// *********************************************
void loop()
{
  RTC.getTime();
  if ( RTC.second != lastSecond )
  // if ( second() != lastSecond )
  {
    lastSecond = RTC.second;
    //lastSecond = second();
  
    // Check before we print because it changes hour to am/pm
    checkYourHead();
        
    printFullTime();
    printTime();
  }

  // Check before the alarm because it messes with time!
  mosfets();
  spkrfet();
  chkAlrm();
  chkAlrmSwitch();
  
  // Read the shift IN
  readSwitches();

  // check the button actions
  checkButtons();
  
  //delay(100);
}

void checkButtons()
{
  // Only allow time changes if the setBtn is pressed AND held
  if ( setBtn ) 
  {
    // If hrBtn is pressed and state changed with enough delay for debounce or held down with enough delay then increase 
    if ( hrBtn && (
        ( hrBtn != hrBtnLast && millis() - hrBtnTime > 20 ) || 
        ( millis() - hrBtnTime ) > 300 )
       )
    {
      hrBtnTime = millis();
      incClock('h');
    }
    if ( hrBtn != hrBtnLast )
    {
      hrBtnTime = millis();
    }
    hrBtnLast = hrBtn;
  
    // If mnBtn is pressed and state changed with enough delay for debounce or held down with enough delay then increase 
    if ( mnBtn && (
        ( mnBtn != mnBtnLast && millis() - mnBtnTime > 20 ) || 
        ( millis() - mnBtnTime ) > 300 )
       )
    {
      mnBtnTime = millis();
      incClock('m');
    }
    if ( mnBtn != mnBtnLast )
    {
      mnBtnTime = millis();
    }
    mnBtnLast = mnBtn;
  }
  
  // if play button is debounced and ready then run it
  if ( playBtn && ( playBtn != playBtnLast && millis() - playBtnTime > 20 ) )
  {
    playBtnTime = millis();
    playSoundButton();
  }
  if ( playBtn != playBtnLast )
  {
    playBtnTime = millis();
  }
  playBtnLast = playBtn;
  
  // if play button is debounced and ready then run it
  if ( snzBtn && ( 
      ( snzBtn != snzBtnLast && millis() - snzBtnTime > 20 ) || 
      ( millis() - snzBtnTime ) > 300 )
     )
  {
    snzBtnTime = millis();
    snooze();
  }
  if ( snzBtn != snzBtnLast )
  {
    snzBtnTime = millis();
  }
  snzBtnLast = snzBtn;  
}

void checkYourHead()
{
  // The head should be off between 19:00 - 07:00
  if ( RTC.hour >= 19 || RTC.hour < 8 )
  //if (hour() >= 19 || hour() < 8 )
  {
    headOn  = 0;
    lightOn = 0;
  } else {
    headOn  = 1;
    lightOn = 1;
  }
}

void spkrfet()
{
  // Speaker was too noisy so I added a FET
  digitalWrite(speakerFETPin, digitalRead(busyPin));
}

void mosfets()
{
  /*
    Push out all the mosfet shiftOut for the mouth, noisemaker, head, light
  */
  // If the noisemaker is on or the busyPin is set turn on the mouth
  if ( noiseOn || digitalRead(busyPin) )
  {
    mouthOn = 1;
  } else {
    mouthOn = 0;
  }
  bitWrite(shftOutValue,0,mouthOn);
  bitWrite(shftOutValue,1,noiseOn);
  bitWrite(shftOutValue,2,headOn);
  bitWrite(shftOutValue,3,lightOn);  
  digitalWrite(shftOutLtchPin, LOW);
  shiftOut(shftOutDataPin, shftOutClkPin, MSBFIRST, shftOutValue);  
  digitalWrite(shftOutLtchPin, HIGH);
}

void readSwitches()
{
  // Flip the latch
  digitalWrite(shftInLtchPin,1);
  delayMicroseconds(20);
  digitalWrite(shftInLtchPin,0);

  byte first4switches;
  byte next4switches;
  byte dip4switches;
  
  // Reset the vars
  alrmFile = 0;
  alrmHour = 0;
  alrmMin  = 0;
  int i;
  int j;
  for (j=1; j<=8; j++)
  {
    byte myDataIn = 0;
    for (i=3; i>=0; i--)
    {
      digitalWrite(shftInClkPin, 0);
      delayMicroseconds(2);
      if ( digitalRead(shftInDataPin) )
      {
        myDataIn = myDataIn | (1 << i);
      }
      digitalWrite(shftInClkPin, 1);
    }
    
    /*
      Switch out how we are handling each digit
      each chip reads 7-0 so numbers are reversed
    */
    switch (j)
    {
      case 1:
        alrmFile = int(myDataIn);
        break;
      case 2:
        if ( int(myDataIn) > 2 )
          myDataIn = 2;
        alrmHour += int(myDataIn) * 10;
        break;
      case 3:
        if ( int(myDataIn) > 9 )
          myDataIn = 9;
        alrmHour += int(myDataIn);
        break;
      case 4:
        if ( int(myDataIn) > 5 )
          myDataIn = 5;
        alrmMin += int(myDataIn) * 10;
        break;
      case 5:
        if ( int(myDataIn) > 9 )
          myDataIn = 9;
        alrmMin += int(myDataIn);
        break;
      case 6:
        first4switches = myDataIn;
        break;
      case 7:
        next4switches = myDataIn;
        break;
      case 8:
        dip4switches = myDataIn;
      default:
        break;
    }
  }
    
  // Sanity check the times
  if ( alrmHour > 23 )
    alrmHour = 23;
  
  // Process the switches  
  alrmSwitch = first4switches & 1;
  snzBtn     = ( first4switches & 2 ) == 2;
  hrBtn      = ( first4switches & 4 ) == 4;
  mnBtn      = ( first4switches & 8 ) == 8;
  playBtn    = next4switches & 1;
  setBtn     = ( next4switches & 2 ) == 2;
  // I wired these backwards, so first button is actually 8
  snzUnlmt   = ( dip4switches & 8 ) == 8;
}

// SOMO methods
void resetSomo()
{
  digitalWrite(resetPin, LOW);
  delay(50);
  digitalWrite(resetPin, HIGH);
}

void stopSound()
{
  sendCommand(STOP);  
}

void playSound(unsigned int sf)
{
  if ( ! digitalRead(busyPin) )
  { 
    sendCommand(alrmFile);
  }
}

void playSoundButton()
{
  // Sound file pot number
  int filePotNum = filePotMin;

  // Read the pot and map it for the file number
  filePotNum = map( analogRead(filePotPin), 0, 1023, filePotMin, filePotMax );
  // Button press trumps busy
  stopSound();
  delay(50);
  sendCommand(filePotNum);
}

void sendCommand(unsigned int command)
{
  //Serial.println(command);
  
  // start bit
  digitalWrite(clockPin, LOW);
  delay(1);

  for (unsigned int mask = 0x8000; mask > 0; mask >>= 1) {
    if (command & mask) {
      digitalWrite(dataPin, HIGH);
    }
    else {
      digitalWrite(dataPin, LOW);
    }
    // clock low
    digitalWrite(clockPin, LOW);
    delayMicroseconds(200);

    // clock high
    digitalWrite(clockPin, HIGH);
    delayMicroseconds(200);
  }
  // stop bit
  delay(1);
}

void chkAlrmSwitch()
{
  if ( alrmSwitch == 0 )
  {
    if ( alrmOn > 0 )
    {
      stopSound();
    }
    // Reset all the variables if the switch is off
    alrmOn = 0;
    snzCnt = 0;
    snzTimeCur = snzTime;
  }
}

void chkAlrm()
{ 
  if ( alrmSwitch > 0 && (
         ( alrmOn == 0 && alrmHour == RTC.hour && alrmMin == RTC.minute ) || // Start the alarm when the hour:min matches
         ( alrmOn == 2 && alrmSec > 0 && alrmSec < RTC.time2000 ) // or if we are snoozing and snooze time has passed
         /*
         ( alrmOn == 0 && alrmHour == hour() && alrmMin == minute() ) || // Start the alarm when the hour:min matches
         ( alrmOn == 2 && alrmSec > 0 && alrmSec < now() ) // or if we are snoozing and snooze time has passed
         */
       ) )
  {
    alrmOn=1;
  }
  
  // Decide what to do if it is on
  if ( alrmOn == 1 )
  {
    if ( alrmFile == 0 )
    {
      noiseOn=1;
    } else {     
      playSound(alrmFile);
    }    
  } else {
    // Turn off the noisemaker, unless they are holding the snooze button down
    if ( ! snzBtn )
    {
      noiseOn=0;
    }
  }
}

void snooze()
{
  if ( alrmOn > 0 )
  {
    alrmOn = 2; // Note that we are snoozing
    snzCnt++;
    if ( snzTimeCur > 0 )
    {
      stopSound();
      alrmSec = RTC.time2000 + ( snzTimeCur * 60 );
      //alrmSec = now() + ( snzTimeCur * 60 );
      
      // Speed up the snooze time
      if ( snzCnt > snzLmt && ! snzUnlmt )
      {
        snzTimeCur -= snzDegrade;
      }
    }
  } else {
    printTime();
    // TODO : this needs to turn on noisemaker OR play file
    if ( alrmFile == 0 )
    {
      noiseOn=1;
    } else {     
      playSound(alrmFile);
    } 
  }
}

void incClock(char field)
{
  RTC.getTime();
  if ( field == 'h' ) 
  { 
    //newHour = hour() + 1;
    //if ( newHour >= 24 ) newHour = 0;
    RTC.hour += 1;
    if ( RTC.hour >= 24 ) RTC.hour = 0;
  } 
  else if ( field == 'm' )
  {
    //newMinute = minute() + 1;
    //if ( newMinute > 59 ) newMinute = 0;
    RTC.minute +=1;
    if ( RTC.minute > 59 ) RTC.minute = 0;
  }
  //setTime(newHour,newMinute,00,7,3,2010);
  
  RTC.stopClock();
  RTC.fillByHMS(RTC.hour,RTC.minute,0);
  RTC.setTime();
  RTC.startClock();
  
  lastSecond = 99;
}

void printTime()
{
  matrix.clear();
  
  if ( snzBtn )
  {
    disHour = alrmHour;
    disMin  = alrmMin;
  } else {
    //disHour = hour();
    //disMin = minute();
    disHour = RTC.hour;
    disMin  = RTC.minute;
  }

  showColon = 1 - showColon;
  matrix.drawColon(showColon);

  if ( disHour > 11 )
  {
    isPMVar = true;
    disHour -= 12;
  } else {
    isPMVar = false;
  }
  if ( disHour == 0 ) disHour = 12;
  
  if ( int(disHour/10) > 0 )
    matrix.writeDigitNum(0,int(disHour/10),0);
  matrix.writeDigitNum(1,(disHour%10),0);
  
  matrix.writeDigitNum(3,int(disMin/10),0);
  matrix.writeDigitNum(4,(disMin%10),isPMVar);
  
  //
  matrix.setBrightness(map(constrain(analogRead(lightPin),200,1000),200,1000,0,10));
  matrix.writeDisplay();
}

/*
  Used for debugging only
*/
void printFullTime()
{
  if (RTC.hour < 10)                    // correct hour if necessary
  {
    Serial.print("0");
    Serial.print(RTC.hour, DEC);
  } 
  else
  {
    Serial.print(RTC.hour, DEC);
  }
  Serial.print(":");
  if (RTC.minute < 10)                  // correct minute if necessary
  {
    Serial.print("0");
    Serial.print(RTC.minute, DEC);
  }
  else
  {
    Serial.print(RTC.minute, DEC);
  }
  Serial.print(":");
  if (RTC.second < 10)                  // correct second if necessary
  {
    Serial.print("0");
    Serial.print(RTC.second, DEC);
  }
  else
  {
    Serial.print(RTC.second, DEC);
  }
  Serial.print(" ");
  
  if (RTC.month < 10)                   // correct month if necessary
  {
    Serial.print("0");
    Serial.print(RTC.month, DEC);
  }
  else
  {
    Serial.print(RTC.month, DEC);
  }
  Serial.print("/");
  if (RTC.day < 10)                    // correct date if necessary
  {
    Serial.print("0");
    Serial.print(RTC.day, DEC);
  }
  else
  {
    Serial.print(RTC.day, DEC);
  }
  Serial.print("/");
  Serial.print(RTC.year, DEC);          // Year need not to be changed
  Serial.print(" ");
  switch (RTC.dow)                      // Friendly printout the weekday
  {
    case 1:
      Serial.print("MON");
      break;
    case 2:
      Serial.print("TUE");
      break;
    case 3:
      Serial.print("WED");
      break;
    case 4:
      Serial.print("THU");
      break;
    case 5:
      Serial.print("FRI");
      break;
    case 6:
      Serial.print("SAT");
      break;
    case 7:
      Serial.print("SUN");
      break;
  }

  Serial.println();
}
