const String FirmwareVersion = "019500";
const char HardwareVersion[] PROGMEM = {"NCS314 for HW 2.x HV5122 or HV5222"};
//Format                _X.XXX_
// Removed GPS, IR, added NTP, timezones and webserver
// 1.95 20.01.2023
//NIXIE CLOCK SHIELD NCS314 v 2.x by GRA & AFCH (fominalec@gmail.com)

//#define tubes8
#define tubes6
//#define tubes4

#include <SPI.h>
#include <Wire.h>
#include "ClickButton.h"
#include <TimeLib.h>
#ifndef GRA_AND_AFCH_TIME_LIB_MOD
  #error The "Time (TimeLib)" library modified by GRA and AFCH must be used!
#endif
#include <Tone.h>
#include <EEPROM.h>
#include "doIndication314_HW2.x.h"
#include <OneWire.h>
#include <WiFiEspAT.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>
#include <Timezone.h>

#include "arduino_secrets.h"



bool initialBootDone = false;
unsigned long previousMillis_1 = 0, previousMillis_2 = 0;

int ModeButtonState = 0;
int UpButtonState = 0;
int DownButtonState = 0;

boolean UD, LD; // DOTS control;

byte data[12];
byte addr[8];
int celsius, fahrenheit;

const byte RedLedPin = 9; //MCU WDM output for red LEDs 9-g
const byte GreenLedPin = 6; //MCU WDM output for green LEDs 6-b
const byte BlueLedPin = 3; //MCU WDM output for blue LEDs 3-r
const byte pinSet = A0;
const byte pinUp = A2;
const byte pinDown = A1;
const byte pinBuzzer = 2;
//const byte pinBuzzer = 0;
const byte pinUpperDots = 12; //HIGH value light a dots
const byte pinLowerDots = 8; //HIGH value light a dots
const byte pinTemp = 7;
bool RTC_present;
#define US_DateFormat 1
#define EU_DateFormat 0
//bool DateFormat=EU_DateFormat;

OneWire ds(pinTemp);
bool TempPresent = false;
#define CELSIUS 0
#define FAHRENHEIT 1

String stringToDisplay = "000000"; // Conten of this string will be displayed on tubes (must be 6 chars length)
int menuPosition = 0;
// 0 - time
// 1 - date
// 2 - alarm
// 3 - 12/24 hours mode
// 4 - Temperature

byte blinkMask = B00000000; //bit mask for blinkin digits (1 - blink, 0 - constant light)
int blankMask = B00000000; //bit mask for digits (1 - off, 0 - on)

byte dotPattern = B00000000; //bit mask for separeting dots (1 - on, 0 - off)
//B10000000 - upper dots
//B01000000 - lower dots

#define DS1307_ADDRESS 0x68
byte zero = 0x00; //workaround for issue #527
int RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year, RTC_day_of_week;

#define TimeIndex        0
#define DateIndex        1
#define AlarmIndex       2
#define hModeIndex       3
#define TemperatureIndex 4
#define TimeZoneIndex    5
#define TimeHoursIndex   6
#define TimeMintuesIndex 7
#define TimeSecondsIndex 8
#define DateFormatIndex  9
#define DateDayIndex     10
#define DateMonthIndex   11
#define DateYearIndex    12
#define AlarmHourIndex   13
#define AlarmMinuteIndex 14
#define AlarmSecondIndex 15
#define Alarm01          16
#define hModeValueIndex  17
#define DegreesFormatIndex 18
#define HoursOffsetIndex 19

#define FirstParent      TimeIndex
#define LastParent       TimeZoneIndex
#define SettingsCount    (HoursOffsetIndex+1)
#define NoParent         0
#define NoChild          0

//-------------------------------0--------1--------2-------3--------4--------5--------6--------7--------8--------9----------10-------11---------12---------13-------14-------15---------16---------17--------18----------19
//                     names:  Time,   Date,   Alarm,   12/24, Temperature,TimeZone,hours,   mintues, seconds, DateFormat, day,    month,   year,      hour,   minute,   second alarm01  hour_format Deg.FormIndex HoursOffset
//                               1        1        1       1        1        1        1        1        1        1          1        1          1          1        1        1        1            1         1        1
int parent[SettingsCount] = {NoParent, NoParent, NoParent, NoParent, NoParent, NoParent, 1,       1,       1,       2,         2,       2,         2,         3,       3,       3,       3,       4,           5,        6};
int firstChild[SettingsCount] = {6,       9,       13,     17,      18,      19,      0,       0,       0,    NoChild,      0,       0,         0,         0,       0,       0,       0,       0,           0,        0};
int lastChild[SettingsCount] = { 8,      12,       16,     17,      18,      19,      0,       0,       0,    NoChild,      0,       0,         0,         0,       0,       0,       0,       0,           0,        0};
int value[SettingsCount] = {     0,       0,       0,      0,       0,       0,       0,       0,       0,  EU_DateFormat,  0,       0,         0,         0,       0,       0,       0,       24,          0,        2};
int maxValue[SettingsCount] = {  0,       0,       0,      0,       0,       0,       23,      59,      59, US_DateFormat,  31,      12,        99,       23,      59,      59,       1,       24,     FAHRENHEIT,    14};
int minValue[SettingsCount] = {  0,       0,       0,      12,      0,       0,       00,      00,      00, EU_DateFormat,  1,       1,         00,       00,      00,      00,       0,       12,      CELSIUS,     -12};
int blinkPattern[SettingsCount] = {
  B00000000, //0
  B00000000, //1
  B00000000, //2
  B00000000, //3
  B00000000, //4
  B00000000, //5
  B00000011, //6
  B00001100, //7
  B00110000, //8
  B00111111, //9
  B00000011, //10
  B00001100, //11
  B00110000, //12
  B00000011, //13
  B00001100, //14
  B00110000, //15
  B11000000, //16
  B00001100, //17
  B00111111, //18
  B00000011, //19
};

bool editMode = false;

long downTime = 0;
long upTime = 0;
const long settingDelay = 150;
bool BlinkUp = false;
bool BlinkDown = false;
unsigned long enteringEditModeTime = 0;
bool RGBLedsOn = true;
byte RGBLEDsEEPROMAddress = 0;
byte HourFormatEEPROMAddress = 1;
byte AlarmTimeEEPROMAddress = 2; //3,4,5
byte AlarmArmedEEPROMAddress = 6;
byte LEDsLockEEPROMAddress = 7;
byte LEDsRedValueEEPROMAddress = 8;
byte LEDsGreenValueEEPROMAddress = 9;
byte LEDsBlueValueEEPROMAddress = 10;
byte DegreesFormatEEPROMAddress = 11;
byte HoursOffsetEEPROMAddress = 12;
byte DateFormatEEPROMAddress = 13;
byte DotsModeEEPROMAddress = 14;
#define DOT_MODE_314 0
//#define DOT_MODE_312 1
bool DotsMode=DOT_MODE_314;

uint32_t UpperDotsMask=0x80000000;
uint32_t LowerDotsMask=0x40000000;

//buttons pins declarations
ClickButton setButton(pinSet, LOW, CLICKBTN_PULLUP);
ClickButton upButton(pinUp, LOW, CLICKBTN_PULLUP);
ClickButton downButton(pinDown, LOW, CLICKBTN_PULLUP);
///////////////////

Tone tone1;
#define isdigit(n) (n >= '0' && n <= '9')
//char *song = "MissionImp:d=16,o=6,b=95:32d,32d#,32d,32d#,32d,32d#,32d,32d#,32d,32d,32d#,32e,32f,32f#,32g,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,a#,g,2d,32p,a#,g,2c#,32p,a#,g,2c,a#5,8c,2p,32p,a#5,g5,2f#,32p,a#5,g5,2f,32p,a#5,g5,2e,d#,8d";
//char *song = "PinkPanther:d=4,o=5,b=160:8d#,8e,2p,8f#,8g,2p,8d#,8e,16p,8f#,8g,16p,8c6,8b,16p,8d#,8e,16p,8b,2a#,2p,16a,16g,16e,16d,2e";
//char *song="VanessaMae:d=4,o=6,b=70:32c7,32b,16c7,32g,32p,32g,32p,32d#,32p,32d#,32p,32c,32p,32c,32p,32c7,32b,16c7,32g#,32p,32g#,32p,32f,32p,16f,32c,32p,32c,32p,32c7,32b,16c7,32g,32p,32g,32p,32d#,32p,32d#,32p,32c,32p,32c,32p,32g,32f,32d#,32d,32c,32d,32d#,32c,32d#,32f,16g,8p,16d7,32c7,32d7,32a#,32d7,32a,32d7,32g,32d7,32d7,32p,32d7,32p,32d7,32p,16d7,32c7,32d7,32a#,32d7,32a,32d7,32g,32d7,32d7,32p,32d7,32p,32d7,32p,32g,32f,32d#,32d,32c,32d,32d#,32c,32d#,32f,16c";
char *song="DasBoot:d=4,o=5,b=100:d#.4,8d4,8c4,8d4,8d#4,8g4,a#.4,8a4,8g4,8a4,8a#4,8d,2f.,p,f.4,8e4,8d4,8e4,8f4,8a4,c.,8b4,8a4,8b4,8c,8e,2g.,2p";
//char *song="Scatman:d=4,o=5,b=200:8b,16b,32p,8b,16b,32p,8b,2d6,16p,16c#.6,16p.,8d6,16p,16c#6,8b,16p,8f#,2p.,16c#6,8p,16d.6,16p.,16c#6,16b,8p,8f#,2p,32p,2d6,16p,16c#6,8p,16d.6,16p.,16c#6,16a.,16p.,8e,2p.,16c#6,8p,16d.6,16p.,16c#6,16b,8p,8b,16b,32p,8b,16b,32p,8b,2d6,16p,16c#.6,16p.,8d6,16p,16c#6,8b,16p,8f#,2p.,16c#6,8p,16d.6,16p.,16c#6,16b,8p,8f#,2p,32p,2d6,16p,16c#6,8p,16d.6,16p.,16c#6,16a.,16p.,8e,2p.,16c#6,8p,16d.6,16p.,16c#6,16a,8p,8e,2p,32p,16f#.6,16p.,16b.,16p.";
//char *song="Popcorn:d=4,o=5,b=160:8c6,8a#,8c6,8g,8d#,8g,c,8c6,8a#,8c6,8g,8d#,8g,c,8c6,8d6,8d#6,16c6,8d#6,16c6,8d#6,8d6,16a#,8d6,16a#,8d6,8c6,8a#,8g,8a#,c6";
//char *song="WeWishYou:d=4,o=5,b=200:d,g,8g,8a,8g,8f#,e,e,e,a,8a,8b,8a,8g,f#,d,d,b,8b,8c6,8b,8a,g,e,d,e,a,f#,2g,d,g,8g,8a,8g,8f#,e,e,e,a,8a,8b,8a,8g,f#,d,d,b,8b,8c6,8b,8a,g,e,d,e,a,f#,1g,d,g,g,g,2f#,f#,g,f#,e,2d,a,b,8a,8a,8g,8g,d6,d,d,e,a,f#,2g";
#define OCTAVE_OFFSET 0
char *p;

int notes[] = { 0,
                NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
                NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
                NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6,
                NOTE_C7, NOTE_CS7, NOTE_D7, NOTE_DS7, NOTE_E7, NOTE_F7, NOTE_FS7, NOTE_G7, NOTE_GS7, NOTE_A7, NOTE_AS7, NOTE_B7
              };

int fireforks[] = {0, 0, 1, //1
                   -1, 0, 0, //2
                   0, 1, 0, //3
                   0, 0, -1, //4
                   1, 0, 0, //5
                   0, -1, 0
                  }; //array with RGB rules (0 - do nothing, -1 - decrese, +1 - increse

void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w = 1);

int functionDownButton = 0;
int functionUpButton = 0;
bool LEDsLock = false;

//antipoisoning transaction
bool modeChangedByUser = false;
bool transactionInProgress = false; //antipoisoning transaction
#define timeModePeriod 60000
#define dateModePeriod 5000
long modesChangePeriod = timeModePeriod;
//end of antipoisoning transaction

extern const int LEDsDelay;

/*  Wireless settings
 *
*/

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiUDP ntpUDP;
WiFiServer server(80);  // Set web server port

// You can specify the time server pool and the offset (in seconds, can be changed later with setTimeOffset()).
//Additionally you can specify the update interval (in milliseconds, can be changed using setUpdateInterval()).
NTPClient timeClient(ntpUDP, NTPSERVER);
unsigned long epoch = 0;
String tempRTCTime = "", tempCLKTime = "", tempNTPTime = "";
TimeChangeRule DST = TZ_DST;
TimeChangeRule DEF = TZ_DEF;
Timezone myTZ(DST, DEF);
TimeChangeRule *tcr;        // pointer to the time change rule, use to get TZ abbrev
String DSTEnabled = "Unknown";

/*******************************************************************************************************
  Init Programm
*******************************************************************************************************/
void setup()
{
  Serial.begin(115200);
  Wire.begin();
  
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  WiFiSetup();
  server.begin();
#endif

  if (EEPROM.read(HourFormatEEPROMAddress) != 12) value[hModeValueIndex] = 24; else value[hModeValueIndex] = 12;
  if (EEPROM.read(RGBLEDsEEPROMAddress) != 0) RGBLedsOn = true; else RGBLedsOn = false;
  if (EEPROM.read(AlarmTimeEEPROMAddress) == 255) value[AlarmHourIndex] = 0; else value[AlarmHourIndex] = EEPROM.read(AlarmTimeEEPROMAddress);
  if (EEPROM.read(AlarmTimeEEPROMAddress + 1) == 255) value[AlarmMinuteIndex] = 0; else value[AlarmMinuteIndex] = EEPROM.read(AlarmTimeEEPROMAddress + 1);
  if (EEPROM.read(AlarmTimeEEPROMAddress + 2) == 255) value[AlarmSecondIndex] = 0; else value[AlarmSecondIndex] = EEPROM.read(AlarmTimeEEPROMAddress + 2);
  if (EEPROM.read(AlarmArmedEEPROMAddress) == 255) value[Alarm01] = 0; else value[Alarm01] = EEPROM.read(AlarmArmedEEPROMAddress);
  if (EEPROM.read(LEDsLockEEPROMAddress) == 255) LEDsLock = false; else LEDsLock = EEPROM.read(LEDsLockEEPROMAddress);
  if (EEPROM.read(DegreesFormatEEPROMAddress) == 255) value[DegreesFormatIndex] = CELSIUS; else value[DegreesFormatIndex] = EEPROM.read(DegreesFormatEEPROMAddress);
  if (EEPROM.read(HoursOffsetEEPROMAddress) == 255) value[HoursOffsetIndex] = value[HoursOffsetIndex]; else value[HoursOffsetIndex] = EEPROM.read(HoursOffsetEEPROMAddress) + minValue[HoursOffsetIndex];
  if (EEPROM.read(DateFormatEEPROMAddress) == 255) value[DateFormatIndex] = value[DateFormatIndex]; else value[DateFormatIndex] = EEPROM.read(DateFormatEEPROMAddress);

  Serial.println((String)"LED lock = " + LEDsLock);

  pinMode(RedLedPin, OUTPUT);
  pinMode(GreenLedPin, OUTPUT);
  pinMode(BlueLedPin, OUTPUT);

  tone1.begin(pinBuzzer);
  song = parseSong(song);

  pinMode(LEpin, OUTPUT);

  // SPI setup
  SPISetup();
  LEDsSetup();
  //buttons pins inits
  pinMode(pinSet,  INPUT_PULLUP);
  pinMode(pinUp,  INPUT_PULLUP);
  pinMode(pinDown,  INPUT_PULLUP);
  ////////////////////////////
  pinMode(pinBuzzer, OUTPUT);

  //buttons objects inits
  setButton.debounceTime   = 20;   // Debounce timer in ms
  setButton.multiclickTime = 30;  // Time limit for multi clicks
  setButton.longClickTime  = 2000; // time until "held-down clicks" register

  upButton.debounceTime   = 20;   // Debounce timer in ms
  upButton.multiclickTime = 30;  // Time limit for multi clicks
  upButton.longClickTime  = 2000; // time until "held-down clicks" register

  downButton.debounceTime   = 20;   // Debounce timer in ms
  downButton.multiclickTime = 30;  // Time limit for multi clicks
  downButton.longClickTime  = 2000; // time until "held-down clicks" register

  if (EEPROM.read(DotsModeEEPROMAddress) != 255) DotsMode=EEPROM.read(DotsModeEEPROMAddress);
  if (digitalRead(pinDown) == LOW) 
  {
    DotsMode=!DotsMode;
    EEPROM.write(DotsModeEEPROMAddress, DotsMode);
    tone1.play(1000, 100);
  }
  if (DotsMode == DOT_MODE_314)
  {
    UpperDotsMask=0x80000000;
    LowerDotsMask=0x40000000; 
  }
  else
  {
    UpperDotsMask=0x40000000;
    LowerDotsMask=0x80000000;
  }
  
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  doTest();
  //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  if (LEDsLock == 1)
  {
    setLEDsFromEEPROM();
  }
  getRTCTime();
  byte prevSeconds = RTC_seconds;
  unsigned long RTC_ReadingStartTime = millis();
  RTC_present = true;
  while (prevSeconds == RTC_seconds)
  {
    getRTCTime();
    //Serial.println(RTC_seconds);
    if ((millis() - RTC_ReadingStartTime) > 3000)
    {
      Serial.println(F("Warning! RTC DON'T RESPOND!"));
      RTC_present = false;
      break;
    }
  }
  setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  timeClient.begin();
  timeClient.setTimeOffset(HoursOffsetIndex[value] * 3600);
#endif

}

int rotator = 0; //index in array with RGB "rules" (increse by one on each 255 cycles)
int cycle = 0; //cycles counter
int RedLight = 255;
int GreenLight = 0;
int BlueLight = 0;
unsigned long prevTime = 0; // time of lase tube was lit
unsigned long prevTime4FireWorks = 0; //time of last RGB changed

/***************************************************************************************************************
  MAIN Programm
***************************************************************************************************************/
void loop()
{
  WiFiClient client = server.available();

  if(client)
  {
    IPAddress ip = client.remoteIP();
    Serial.println("New client ");
    Serial.println(ip);
    
    while(client.connected())
    {
      if (client.available())
      {
        String line = client.readStringUntil('\n');
        line.trim();
        Serial.println(line);

        if (line.length() == 0)
        {
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println("");
          
          String html = "";

          html += "<!DOCTYPE html>";
          html += "<html lang='en'>";

            html += "<head>";
              html += "<meta charset='utf-8'>";
              html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
              html += "<title>Nixie Clock</title>";
              html += "<style>";
                html += "table {font-family: arial, sans-serif; border-collapse: collapse; width: 100%;}";
                html += "td, th {border: 1px solid #dddddd; text-align: left; padding: 10px;}";
                html += "tr:nth-child(even) {background-color: #dddddd;}";
              html += "</style>";
            html += "</head>";

            html += "<body>";

              html += "<h1 style='text-align: center;'>Current Time Readings</h1>";
              html += "<table>";
                html += "<tr>";
                  html += "<th>Fetched UNIX Time</th>";
                  html += "<th><a target='_blank' href='https://www.epochconverter.com/?q=" + String(epoch) + "'>" + String(epoch) + "</a></th>";
                html += "</tr>";

                html += "<tr>";
                  html += "<th>Fetched NTP Time (UTC)</th>";
                  html += "<th>" + String(timeClient.getHours()) + ":" + String(timeClient.getMinutes()) + ":" + String(timeClient.getSeconds()) + "</th>";
                html += "</tr>";

                html += "<tr>";
                  html += "<th>Current Nixie Time (TZ)</th>";
                  html += "<th>" + String(hour()) + ":" + String(minute()) + ":" + String(second()) + "</th>";
                html += "</tr>";

                html += "<tr>";
                  html += "<th>Stored RTC Time (TZ)</th>";
                  html += "<th>" + String(RTC_hours) + ":" + String(RTC_minutes) + ":" + String(RTC_seconds) + "</th>";
                html += "</tr>";
                
                html += "<tr>";
                  html += "<th>Hours Offset Index</th>";
                  html += "<th>" + String(HoursOffsetIndex[value]) + "</th>";
                html += "</tr>";

                html += "<tr>";
                  html += "<th>Daylight Saving Time (DST) Enabled?</th>";
                  html += "<th>" + String(DSTEnabled) + "</th>";
                html += "</tr>";

              html += "</table>";

              html += "<p style='text-align: center;'>Firmware = " + String(FirmwareVersion.substring(1, 2)) + "." + String(FirmwareVersion.substring(2, 5)) + "</p>";

            html += "</body>";
          html += "</html>";

          client.println(html);
          client.flush();
          break;
        }
      }
    }
    client.stop();
  }

  // Synchronize from RTC every 10 seconds
  if ((millis() % (10UL * 1000UL) == 0) && (RTC_present))
  {
    getRTCTime();
    setTime(RTC_hours, RTC_minutes, RTC_seconds, RTC_day, RTC_month, RTC_year);
  }

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  
  // Update NTP upon boot
  if (!initialBootDone)
  {
    initialBootDone = true;
    
    Serial.println(F("\n"));
    Serial.println(F("Attempting to sync with NTP after boot"));

    //timeClient.update();
    timeClient.forceUpdate();
    
    if(timeClient.isTimeSet())
    {
      setNTPTime();
      Serial.println(F("NTP updated after boot"));
    }    
  }

  //synchronize with NTP every 1.5 minutes (90UL * 1000UL)
  else if ((millis() - previousMillis_2) >= (90UL * 1000UL))
  {
    // Reset previousMillis to current millis
    previousMillis_2 = millis();

    Serial.println(F("\n"));
    Serial.println(F("Attempting to sync with NTP"));

    timeClient.forceUpdate();

    if(timeClient.isTimeSet())
    {
      setNTPTime();
      Serial.println(F("NTP updated"));
    }
  }

  ModeButtonState = 0;
  UpButtonState = 0;
  DownButtonState = 0;

#else
  ModeButtonState = 0;
  UpButtonState = 0;
  DownButtonState = 0;
#endif

  p = playmusic(p);

  if ((millis() - prevTime4FireWorks) > LEDsDelay)
  {
    rotateFireWorks(); //change color (by 1 step)
    prevTime4FireWorks = millis();
  }

  if ((menuPosition == TimeIndex) || (modeChangedByUser == false) ) modesChanger();
  doIndication();

  setButton.Update();
  upButton.Update();
  downButton.Update();
  if (editMode == false)
  {
    blinkMask = B00000000;

  } else if ((millis() - enteringEditModeTime) > 60000)
  {
    editMode = false;
    menuPosition = firstChild[menuPosition];
    blinkMask = blinkPattern[menuPosition];
  }
  if ((setButton.clicks > 0) || (ModeButtonState == 1)) //short click
  {
    modeChangedByUser = true;
    p = 0; //shut off music )))
    tone1.play(1000, 100);
    enteringEditModeTime = millis();
    menuPosition = menuPosition + 1;
#if defined (__AVR_ATmega328P__)
    if (menuPosition == TimeZoneIndex) menuPosition++;// skip TimeZone for Arduino Uno
#endif
    if (menuPosition == LastParent + 1) menuPosition = TimeIndex;
    /*Serial.print(F("menuPosition="));
    Serial.println(menuPosition);
    Serial.print(F("value="));
    Serial.println(value[menuPosition]);*/

    blinkMask = blinkPattern[menuPosition];
    if ((parent[menuPosition - 1] != 0) and (lastChild[parent[menuPosition - 1] - 1] == (menuPosition - 1))) //exit from edit mode
    {
      if ((parent[menuPosition - 1] - 1 == 1) && (!isValidDate()))
      {
        menuPosition = DateDayIndex;
        return;
      }
      editMode = false;
      menuPosition = parent[menuPosition - 1] - 1;
      if (menuPosition == TimeIndex) setTime(value[TimeHoursIndex], value[TimeMintuesIndex], value[TimeSecondsIndex], day(), month(), year());
      if (menuPosition == DateIndex)
      {
        /*Serial.print("Day:");
        Serial.println(value[DateDayIndex]);
        Serial.print("Month:");
        Serial.println(value[DateMonthIndex]);*/
        setTime(hour(), minute(), second(), value[DateDayIndex], value[DateMonthIndex], 2000 + value[DateYearIndex]);
        EEPROM.write(DateFormatEEPROMAddress, value[DateFormatIndex]);
      }
      if (menuPosition == AlarmIndex) {
        EEPROM.write(AlarmTimeEEPROMAddress, value[AlarmHourIndex]);
        EEPROM.write(AlarmTimeEEPROMAddress + 1, value[AlarmMinuteIndex]);
        EEPROM.write(AlarmTimeEEPROMAddress + 2, value[AlarmSecondIndex]);
        EEPROM.write(AlarmArmedEEPROMAddress, value[Alarm01]);
      };
      if (menuPosition == hModeIndex) EEPROM.write(HourFormatEEPROMAddress, value[hModeValueIndex]);
      if (menuPosition == TemperatureIndex)
      {
        EEPROM.write(DegreesFormatEEPROMAddress, value[DegreesFormatIndex]);
      }
      if (menuPosition == TimeZoneIndex) EEPROM.write(HoursOffsetEEPROMAddress, value[HoursOffsetIndex] - minValue[HoursOffsetIndex]);
      //if (menuPosition == hModeIndex) EEPROM.write(HourFormatEEPROMAddress, value[hModeValueIndex]);
      setRTCDateTime(hour(), minute(), second(), day(), month(), year() % 1000, 1);
      return;
    } //end exit from edit mode
    /*Serial.print("menu pos=");
    Serial.println(menuPosition);
    Serial.print("DateFormat");
    Serial.println(value[DateFormatIndex]);*/
    if ((menuPosition != HoursOffsetIndex) &&
        (menuPosition != DateFormatIndex) &&
        (menuPosition != DateDayIndex)) value[menuPosition] = extractDigits(blinkMask);
  }
  if ((setButton.clicks < 0) || (ModeButtonState == -1)) //long click
  {
    tone1.play(1000, 100);
    if (!editMode)
    {
      enteringEditModeTime = millis();
      if (menuPosition == TimeIndex) stringToDisplay = PreZero(hour()) + PreZero(minute()) + PreZero(second()); //temporary enabled 24 hour format while settings
    }
    if (menuPosition == DateIndex)
    {
      //Serial.println("DateEdit");
      value[DateDayIndex] =  day();
      value[DateMonthIndex] = month();
      value[DateYearIndex] = year() % 1000;
      if (value[DateFormatIndex] == EU_DateFormat) stringToDisplay = PreZero(value[DateDayIndex]) + PreZero(value[DateMonthIndex]) + PreZero(value[DateYearIndex]);
      else stringToDisplay = PreZero(value[DateMonthIndex]) + PreZero(value[DateDayIndex]) + PreZero(value[DateYearIndex]);
      //Serial.print("str=");
      //Serial.println(stringToDisplay);
    }
    menuPosition = firstChild[menuPosition];
    if (menuPosition == AlarmHourIndex) {
      value[Alarm01] = 1; /*digitalWrite(pinUpperDots, HIGH);*/dotPattern = B10000000;
    }
    editMode = !editMode;
    blinkMask = blinkPattern[menuPosition];
    if ((menuPosition != DegreesFormatIndex) &&
        (menuPosition != HoursOffsetIndex) &&
        (menuPosition != DateFormatIndex))
      value[menuPosition] = extractDigits(blinkMask);
    /*Serial.print(F("menuPosition="));
    Serial.println(menuPosition);
    Serial.print(F("value="));
    Serial.println(value[menuPosition]);*/
  }

  if (upButton.clicks != 0) functionUpButton = upButton.clicks;

  if ((upButton.clicks > 0) || (UpButtonState == 1))
  {
    modeChangedByUser = true;
    p = 0; //shut off music )))
    tone1.play(1000, 100);
    incrementValue();
    if (!editMode)
    {
      LEDsLock = false;
      EEPROM.write(LEDsLockEEPROMAddress, 0);
    }
  }

  if (functionUpButton == -1 && upButton.depressed == true)
  {
    BlinkUp = false;
    if (editMode == true)
    {
      if ( (millis() - upTime) > settingDelay)
      {
        upTime = millis();// + settingDelay;
        incrementValue();
      }
    }
  } else BlinkUp = true;

  if (downButton.clicks != 0) functionDownButton = downButton.clicks;

  if ((downButton.clicks > 0) || (DownButtonState == 1))
  {
    modeChangedByUser = true;
    p = 0; //shut off music )))
    tone1.play(1000, 100);
    dicrementValue();
    if (!editMode)
    {
      LEDsLock = true;
      EEPROM.write(LEDsLockEEPROMAddress, 1);
      EEPROM.write(LEDsRedValueEEPROMAddress, RedLight);
      EEPROM.write(LEDsGreenValueEEPROMAddress, GreenLight);
      EEPROM.write(LEDsBlueValueEEPROMAddress, BlueLight);
      /*Serial.println(F("Store to EEPROM:"));
      Serial.print(F("RED="));
      Serial.println(RedLight);
      Serial.print(F("GREEN="));
      Serial.println(GreenLight);
      Serial.print(F("Blue="));
      Serial.println(BlueLight);*/
    }
  }

  if (functionDownButton == -1 && downButton.depressed == true)
  {
    BlinkDown = false;
    if (editMode == true)
    {
      if ( (millis() - downTime) > settingDelay)
      {
        downTime = millis();// + settingDelay;
        dicrementValue();
      }
    }
  } else BlinkDown = true;

  if (!editMode)
  {
    if ((upButton.clicks < 0) || (UpButtonState == -1))
    {
      tone1.play(1000, 100);
      RGBLedsOn = true;
      EEPROM.write(RGBLEDsEEPROMAddress, 1);
      //Serial.println(F("RGB=on"));
      setLEDsFromEEPROM();
    }
    if ((downButton.clicks < 0) || (DownButtonState == -1))
    {
      tone1.play(1000, 100);
      RGBLedsOn = false;
      EEPROM.write(RGBLEDsEEPROMAddress, 0);
      //Serial.println(F("RGB=off"));
    }
  }

  static bool updateDateTime = false;
  switch (menuPosition)
  {
    case TimeIndex: //time mode
      if (!transactionInProgress) stringToDisplay = updateDisplayString();
      doDotBlink();
      checkAlarmTime();
      blankMask = B00000000;
      break;
    case DateIndex: //date mode
      if (!transactionInProgress) stringToDisplay = updateDateString();
      dotPattern = B01000000; //turn on lower dots
      checkAlarmTime();
      blankMask = B00000000;
      break;
    case AlarmIndex: //alarm mode
      stringToDisplay = PreZero(value[AlarmHourIndex]) + PreZero(value[AlarmMinuteIndex]) + PreZero(value[AlarmSecondIndex]);
      blankMask = B00000000;
      if (value[Alarm01] == 1) /*digitalWrite(pinUpperDots, HIGH);*/ dotPattern = B10000000; //turn on upper dots
      else
      {
        dotPattern = B00000000; //turn off upper dots
      }
      checkAlarmTime();
      break;
    case hModeIndex: //12/24 hours mode
      stringToDisplay = "00" + String(value[hModeValueIndex]) + "00";
      blankMask = B00110011;
      dotPattern = B00000000; //turn off all dots
      checkAlarmTime();
      break;
    case TemperatureIndex: //missed break
    case DegreesFormatIndex:
      if (!transactionInProgress)
      {
        stringToDisplay = updateTemperatureString(getTemperature(value[DegreesFormatIndex]));
        if (value[DegreesFormatIndex] == CELSIUS)
        {
          blankMask = B00110001;
          dotPattern = B01000000;
        }
        else
        {
          blankMask = B00100011;
          dotPattern = B00000000;
        }
      }

      if (getTemperature(value[DegreesFormatIndex]) < 0) dotPattern |= B10000000;
      else dotPattern &= B01111111;
      break;
    case TimeZoneIndex:
    case HoursOffsetIndex:
      stringToDisplay = String(PreZero(value[HoursOffsetIndex])) + "0000";
      blankMask = B00001111;
      if (value[HoursOffsetIndex] >= 0) dotPattern = B00000000; //turn off all dots
      else dotPattern = B10000000; //turn on upper dots
      break;
    case DateFormatIndex:
      if (value[DateFormatIndex] == EU_DateFormat)
      {
        stringToDisplay = "311299";
        blinkPattern[DateDayIndex] = B00000011;
        blinkPattern[DateMonthIndex] = B00001100;
      }
      else
      {
        stringToDisplay = "123199";
        blinkPattern[DateDayIndex] = B00001100;
        blinkPattern[DateMonthIndex] = B00000011;
      }
      break;
    case DateDayIndex:
    case DateMonthIndex:
    case DateYearIndex:
      if (value[DateFormatIndex] == EU_DateFormat) stringToDisplay = PreZero(value[DateDayIndex]) + PreZero(value[DateMonthIndex]) + PreZero(value[DateYearIndex]);
      else stringToDisplay = PreZero(value[DateMonthIndex]) + PreZero(value[DateDayIndex]) + PreZero(value[DateYearIndex]);
      break;
  }
}

String PreZero(int digit)
{
  digit = abs(digit);
  if (digit < 10) return String("0") + String(digit);
  else return String(digit);
}

String updateDisplayString()
{
  static int prevS = -1;

  if (second() != prevS)
  {
    prevS = second();
    return getTimeNow();
  } else return stringToDisplay;
}

String getTimeNow()
{
  if (value[hModeValueIndex] == 24) return PreZero(hour()) + PreZero(minute()) + PreZero(second());
  else return PreZero(hourFormat12()) + PreZero(minute()) + PreZero(second());
}

void doTest()
{
  Serial.print(F("Firmware version: "));
  Serial.println(FirmwareVersion.substring(1, 2) + "." + FirmwareVersion.substring(2, 5));
  //Serial.println(PSTR(HardwareVersion));
  for (byte k = 0; k < strlen_P(HardwareVersion); k++) {
    Serial.print((char)pgm_read_byte_near(HardwareVersion + k));
  }
  Serial.println();
  Serial.println(F("Start Test"));

  // Comment below to mute song
  p = song;
  parseSong(p);
  
  LEDsTest();

  #if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
  if(Serial1.available() > 10)
  { 
    Serial.println(F("GPS dongle detected"));
  }
  else
  {
    Serial.println(F("GPS dongle NOT detected!"));
  }

  if(Serial3.available() > 10)
  {
    Serial.println(F("WIFI module detected"));
  }
  else
  {
    Serial.println(F("WiFi module NOT detected!"));
  }
  #endif

#ifdef tubes8
  String testStringArray[11] = {"00000000", "11111111", "22222222", "33333333", "44444444", "55555555", "66666666", "77777777", "88888888", "99999999", ""};
  testStringArray[10] = FirmwareVersion + "00";
#endif
#ifdef tubes6
  String testStringArray[11] = {"000000", "111111", "222222", "333333", "444444", "555555", "666666", "777777", "888888", "999999", ""};
  testStringArray[10] = FirmwareVersion;
#endif

  int dlay = 500;
  bool test = 1;
  byte strIndex = -1;
  unsigned long startOfTest = millis() + 1000; //disable delaying in first iteration
  bool digitsLock = false;
  while (test)
  {
    if (digitalRead(pinDown) == 0) digitsLock = true;
    if (digitalRead(pinUp) == 0) digitsLock = false;

    if ((millis() - startOfTest) > dlay)
    {
      startOfTest = millis();
      if (!digitsLock) strIndex = strIndex + 1;
      if (strIndex == 10) dlay = 2000;
      if (strIndex > 10) {
        test = false;
        strIndex = 10;
      }

      stringToDisplay = testStringArray[strIndex];
      Serial.println(stringToDisplay);
      doIndication();
    }
    delayMicroseconds(2000);
  };

  if ( !ds.search(addr))
  {
    Serial.println(F("Temp. sensor not found."));
  } else TempPresent = true;

  testDS3231TempSensor();

  Serial.println(F("Stop Test"));
  // while(1);
}

void doDotBlink()
{
  //dotPattern = B11000000; return; //always on
  //dotPattern = B00000000; return; //always off
  if (second() % 2 == 0) dotPattern = B11000000;
  else dotPattern = B00000000;
}

void setRTCDateTime(byte h, byte m, byte s, byte d, byte mon, byte y, byte w)
{
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero); //stop Oscillator

  Wire.write(decToBcd(s));
  Wire.write(decToBcd(m));
  Wire.write(decToBcd(h));
  Wire.write(decToBcd(w));
  Wire.write(decToBcd(d));
  Wire.write(decToBcd(mon));
  Wire.write(decToBcd(y));

  Wire.write(zero); //start

  Wire.endTransmission();

}

byte decToBcd(byte val) {
  // Convert normal decimal numbers to binary coded decimal
  return ( (val / 10 * 16) + (val % 10) );
}

byte bcdToDec(byte val)  {
  // Convert binary coded decimal to normal decimal numbers
  return ( (val / 16 * 10) + (val % 16) );
}

void getRTCTime()
{
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(zero);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 7);

  RTC_seconds = bcdToDec(Wire.read());
  RTC_minutes = bcdToDec(Wire.read());
  RTC_hours = bcdToDec(Wire.read() & 0b111111); //24 hour time
  RTC_day_of_week = bcdToDec(Wire.read()); //0-6 -> sunday - Saturday
  RTC_day = bcdToDec(Wire.read());
  RTC_month = bcdToDec(Wire.read());
  RTC_year = bcdToDec(Wire.read());
}

int extractDigits(byte b)
{
  String tmp = "1";

  if (b == B00000011)
  {
    tmp = stringToDisplay.substring(0, 2);
  }
  if (b == B00001100)
  {
    tmp = stringToDisplay.substring(2, 4);
  }
  if (b == B00110000)
  {
    tmp = stringToDisplay.substring(4);
  }
  return tmp.toInt();
}

void injectDigits(byte b, int value)
{
  if (b == B00000011) stringToDisplay = PreZero(value) + stringToDisplay.substring(2);
  if (b == B00001100) stringToDisplay = stringToDisplay.substring(0, 2) + PreZero(value) + stringToDisplay.substring(4);
  if (b == B00110000) stringToDisplay = stringToDisplay.substring(0, 4) + PreZero(value);
}

bool isValidDate()
{
  int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (value[DateYearIndex] % 4 == 0) days[1] = 29;
  if (value[DateDayIndex] > days[value[DateMonthIndex] - 1]) return false;
  else return true;

}

byte default_dur = 4;
byte default_oct = 6;
int bpm = 63;
int num;
long wholenote;
long duration;
byte note;
byte scale;
char* parseSong(char *p)
{
  // Absolutely no error checking in here
  // format: d=N,o=N,b=NNN:
  // find the start (skip name, etc)

  while (*p != ':') p++;   // ignore name
  p++;                     // skip ':'

  // get default duration
  if (*p == 'd')
  {
    p++; p++;              // skip "d="
    num = 0;
    while (isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    if (num > 0) default_dur = num;
    p++;                   // skip comma
  }

  // get default octave
  if (*p == 'o')
  {
    p++; p++;              // skip "o="
    num = *p++ - '0';
    if (num >= 3 && num <= 7) default_oct = num;
    p++;                   // skip comma
  }

  // get BPM
  if (*p == 'b')
  {
    p++; p++;              // skip "b="
    num = 0;
    while (isdigit(*p))
    {
      num = (num * 10) + (*p++ - '0');
    }
    bpm = num;
    p++;                   // skip colon
  }

  // BPM usually expresses the number of quarter notes per minute
  wholenote = (60 * 1000L / bpm) * 4;  // this is the time for whole note (in milliseconds)
  return p;
}

// now begin note loop
static unsigned long lastTimeNotePlaying = 0;
char* playmusic(char *p)
{
  if (*p == 0)
  {
    return p;
  }
  if (millis() - lastTimeNotePlaying > duration)
    lastTimeNotePlaying = millis();
  else return p;
  // first, get note duration, if available
  num = 0;
  while (isdigit(*p))
  {
    num = (num * 10) + (*p++ - '0');
  }

  if (num) duration = wholenote / num;
  else duration = wholenote / default_dur;  // we will need to check if we are a dotted note after

  // now get the note
  note = 0;

  switch (*p)
  {
    case 'c':
      note = 1;
      break;
    case 'd':
      note = 3;
      break;
    case 'e':
      note = 5;
      break;
    case 'f':
      note = 6;
      break;
    case 'g':
      note = 8;
      break;
    case 'a':
      note = 10;
      break;
    case 'b':
      note = 12;
      break;
    case 'p':
    default:
      note = 0;
  }
  p++;

  // now, get optional '#' sharp
  if (*p == '#')
  {
    note++;
    p++;
  }

  // now, get optional '.' dotted note
  if (*p == '.')
  {
    duration += duration / 2;
    p++;
  }

  // now, get scale
  if (isdigit(*p))
  {
    scale = *p - '0';
    p++;
  }
  else
  {
    scale = default_oct;
  }

  scale += OCTAVE_OFFSET;

  if (*p == ',')
    p++;       // skip comma for next note (or we may be at the end)

  // now play the note

  if (note)
  {
    tone1.play(notes[(scale - 4) * 12 + note], duration);
    if (millis() - lastTimeNotePlaying > duration)
      lastTimeNotePlaying = millis();
    else return p;
    tone1.stop();
  }
  else
  {
    return p;
  }
  Serial.println(F("Incorrect Song Format!"));
  return 0; //error
}


void incrementValue()
{
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    if (menuPosition != hModeValueIndex) // 12/24 hour mode menu position
      value[menuPosition] = value[menuPosition] + 1; else value[menuPosition] = value[menuPosition] + 12;
    if (value[menuPosition] > maxValue[menuPosition])  value[menuPosition] = minValue[menuPosition];
    if (menuPosition == Alarm01)
    {
      if (value[menuPosition] == 1) /*digitalWrite(pinUpperDots, HIGH);*/dotPattern = B10000000; //turn on upper dots
      /*else digitalWrite(pinUpperDots, LOW); */ dotPattern = B00000000; //turn off all dots
    }
    if (menuPosition != DateFormatIndex) injectDigits(blinkMask, value[menuPosition]);
    /*Serial.print("value=");
    Serial.println(value[menuPosition]);*/
  }
}

void dicrementValue()
{
  enteringEditModeTime = millis();
  if (editMode == true)
  {
    if (menuPosition != hModeValueIndex) value[menuPosition] = value[menuPosition] - 1; else value[menuPosition] = value[menuPosition] - 12;
    if (value[menuPosition] < minValue[menuPosition]) value[menuPosition] = maxValue[menuPosition];
    if (menuPosition == Alarm01)
    {
      if (value[menuPosition] == 1) /*digitalWrite(pinUpperDots, HIGH);*/ dotPattern = B10000000; //turn on upper dots
      else /*digitalWrite(pinUpperDots, LOW);*/ dotPattern = B00000000; //turn off all dots
    }
    if (menuPosition != DateFormatIndex) injectDigits(blinkMask, value[menuPosition]);
    /*Serial.print("value=");
    Serial.println(value[menuPosition]);*/
  }
}

bool Alarm1SecondBlock = false;
unsigned long lastTimeAlarmTriggired = 0;
void checkAlarmTime()
{
  if (value[Alarm01] == 0) return;
  if ((Alarm1SecondBlock == true) && ((millis() - lastTimeAlarmTriggired) > 1000)) Alarm1SecondBlock = false;
  if (Alarm1SecondBlock == true) return;
  if ((hour() == value[AlarmHourIndex]) && (minute() == value[AlarmMinuteIndex]) && (second() == value[AlarmSecondIndex]))
  {
    lastTimeAlarmTriggired = millis();
    Alarm1SecondBlock = true;
    Serial.println(F("Wake up, Neo!"));
    p = song;
  }
}

void modesChanger()
{
  if (editMode == true) return;
  static unsigned long lastTimeModeChanged = millis();
  static unsigned long lastTimeAntiPoisoningIterate = millis();
  static int transnumber = 0;
  if ((millis() - lastTimeModeChanged) > modesChangePeriod)
  {
    lastTimeModeChanged = millis();
    if (transnumber == 0) {
      menuPosition = DateIndex;
      modesChangePeriod = dateModePeriod;
    }
    if (transnumber == 1) {
      menuPosition = TemperatureIndex;
      modesChangePeriod = dateModePeriod;
      if (!TempPresent) transnumber = 2;
    }
    if (transnumber == 2) {
      menuPosition = TimeIndex;
      modesChangePeriod = timeModePeriod;
    }
    transnumber++;
    if (transnumber > 2) transnumber = 0;

    if (modeChangedByUser == true)
    {
      menuPosition = TimeIndex;
    }
    modeChangedByUser = false;
  }
  if ((millis() - lastTimeModeChanged) < 2000)
  {
    if ((millis() - lastTimeAntiPoisoningIterate) > 100)
    {
      lastTimeAntiPoisoningIterate = millis();
      if (TempPresent)
      {
        if (menuPosition == TimeIndex) stringToDisplay = antiPoisoning2(updateTemperatureString(getTemperature(value[DegreesFormatIndex])), getTimeNow());
        if (menuPosition == DateIndex) stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(day()) + PreZero(month()) + PreZero(year() % 1000) );
        if (menuPosition == TemperatureIndex) stringToDisplay = antiPoisoning2(PreZero(day()) + PreZero(month()) + PreZero(year() % 1000), updateTemperatureString(getTemperature(value[DegreesFormatIndex])));
      } else
      {
        if (menuPosition == TimeIndex) stringToDisplay = antiPoisoning2(PreZero(day()) + PreZero(month()) + PreZero(year() % 1000), getTimeNow());
        if (menuPosition == DateIndex) stringToDisplay = antiPoisoning2(getTimeNow(), PreZero(day()) + PreZero(month()) + PreZero(year() % 1000) );
      }
      // Serial.println("StrTDInToModeChng="+stringToDisplay);
    }
  } else
  {
    transactionInProgress = false;
  }
}

String antiPoisoning2(String fromStr, String toStr)
{
  //static bool transactionInProgress=false;
  //byte fromDigits[6];
  static byte toDigits[6];
  static byte currentDigits[6];
  static byte iterationCounter = 0;
  if (!transactionInProgress)
  {
    transactionInProgress = true;
    blankMask = B00000000;
    for (int i = 0; i < 6; i++)
    {
      currentDigits[i] = fromStr.substring(i, i + 1).toInt();
      toDigits[i] = toStr.substring(i, i + 1).toInt();
    }
  }
  for (int i = 0; i < 6; i++)
  {
    if (iterationCounter < 10) currentDigits[i]++;
    else if (currentDigits[i] != toDigits[i]) currentDigits[i]++;
    if (currentDigits[i] == 10) currentDigits[i] = 0;
  }
  iterationCounter++;
  if (iterationCounter == 20)
  {
    iterationCounter = 0;
    transactionInProgress = false;
  }
  String tmpStr;
  for (int i = 0; i < 6; i++)
    tmpStr += currentDigits[i];
  return tmpStr;
}

String updateDateString()
{
  static unsigned long lastTimeDateUpdate = millis() + 1001;
  static String DateString = PreZero(day()) + PreZero(month()) + PreZero(year() % 1000);
  static byte prevoiusDateFormatWas = value[DateFormatIndex];
  if (((millis() - lastTimeDateUpdate) > 1000) || (prevoiusDateFormatWas != value[DateFormatIndex]))
  {
    lastTimeDateUpdate = millis();
    if (value[DateFormatIndex] == EU_DateFormat) DateString = PreZero(day()) + PreZero(month()) + PreZero(year() % 1000);
    else DateString = PreZero(month()) + PreZero(day()) + PreZero(year() % 1000);
  }
  return DateString;
}

float getTemperature (boolean bTempFormat)
{
  // Sensor type DS18B20
  byte TempRawData[2];
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0x44); //send make convert to all devices
  ds.reset();
  ds.write(0xCC); //skip ROM command
  ds.write(0xBE); //send request to all devices

  TempRawData[0] = ds.read();
  TempRawData[1] = ds.read();
  int16_t raw = (TempRawData[1] << 8) | TempRawData[0];
  if (raw == -1) raw = 0;
  float celsius = (float)raw / 16.0;
  float fDegrees;
  if (!bTempFormat) fDegrees = celsius * 10;
  else fDegrees = (celsius * 1.8 + 32.0) * 10;
  //Serial.println(fDegrees);
  return fDegrees;
}

String updateTemperatureString(float fDegrees)
{
  static  unsigned long lastTimeTemperatureString = millis() + 1100;
  static String strTemp = "000000";
  if ((millis() - lastTimeTemperatureString) > 1000)
  {
    //Serial.println("F(Updating temp. str.)");
    lastTimeTemperatureString = millis();
    int iDegrees = round(fDegrees);
    if (value[DegreesFormatIndex] == CELSIUS)
    {
      strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000) strTemp = "00" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 100) strTemp = "000" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 10) strTemp = "0000" + String(abs(iDegrees)) + "0";
    } else
    {
      strTemp = "0" + String(abs(iDegrees)) + "0";
      if (abs(iDegrees) < 1000) strTemp = "00" + String(abs(iDegrees) / 10) + "00";
      if (abs(iDegrees) < 100) strTemp = "000" + String(abs(iDegrees) / 10) + "00";
      if (abs(iDegrees) < 10) strTemp = "0000" + String(abs(iDegrees) / 10) + "00";
    }

#ifdef tubes8
    strTemp = "" + strTemp + "00";
#endif
    return strTemp;
  }
  return strTemp;
}

void testDS3231TempSensor()
{
  int8_t DS3231InternalTemperature = 0;
  Wire.beginTransmission(DS1307_ADDRESS);
  Wire.write(0x11);
  Wire.endTransmission();

  Wire.requestFrom(DS1307_ADDRESS, 2);
  DS3231InternalTemperature = Wire.read();
  Serial.print(F("DS3231_T="));
  Serial.println(DS3231InternalTemperature);
  if ((DS3231InternalTemperature < 5) || (DS3231InternalTemperature > 60))
  {
    Serial.println(F("Faulty DS3231!"));
    for (int i = 0; i < 5; i++)
    {
      tone1.play(1000, 1000);
      delay(2000);
    }
  }
}

// START defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
void WiFiSetup()
{
  Serial3.begin(115200);
  WiFi.init(&Serial3);    // initialize ESP module
    
  Serial.println(F("\n"));
  Serial.println((String)"Connecting to = " + ssid);
  
  WiFi.begin(ssid, pass);
  
  // Set up Wifi connection
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  // Wifi is connected
  if(WiFi.status() == WL_CONNECTED)
  {  
    Serial.println(F("\n"));

    IPAddress ip = WiFi.localIP();
    Serial.print(F("IP Address: "));
    Serial.println(ip);
  
    Serial.println((String)"Signal strength (RSSI) = " + WiFi.RSSI() + " dBm");
    Serial.println((String)"ESP8266 Firmware = " + WiFi.firmwareVersion());
    Serial.println(F("\n"));
  }
}


// NTP stuff here
void setNTPTime()
{
  // fetch NTP time in Unix time (epoch timestamp)
  epoch = timeClient.getEpochTime();

  // Show fetched NTP time
  Serial.println("UNIX epoch time = " + String(epoch));
  Serial.println("Formatted time = " + String(timeClient.getFormattedTime()));
  // Set the clock to the fetched NTP time + time offset in Unix time (epoch timestamp)
  //setTime(epoch);
  
  // Update time according to timezone
  myTZ.setRules(DST, DEF);
  setTime(myTZ.toLocal(epoch, &tcr));

  if(myTZ.locIsDST(epoch))
  {
    DSTEnabled = "Yes";
  }
  else
  {
    DSTEnabled = "No";
  }
  
  // Logic to compare RTC/NTP/CLK drift and update if needed, prevents excessive RTC writes
  tempRTCTime = String(RTC_hours) + ":" + String(RTC_minutes) + ":" + String(RTC_seconds);
  tempNTPTime = String(timeClient.getHours()) + ":" + String(timeClient.getMinutes()) + ":" + String(timeClient.getSeconds());
  tempCLKTime = String(hour()) + ":" + String(minute()) + ":" + String(second());

  Serial.println("Current RTC time = " + tempRTCTime);
  Serial.println("Current NTP time = " + tempNTPTime);
  Serial.println("Fetched CLK time = " + tempCLKTime);

  if(RTC_hours == hour() && RTC_minutes == minute() && RTC_seconds == second())
  {
    Serial.println(F("Time not synced with RTC, since RTC and current time (CLK) are the same"));
  }
  else
  {
    setRTCDateTime(hour(), minute(), second(), day(), month(), year() % 1000, weekday());
    Serial.println(F("Updated current RTC time to current clock time"));
  }
}

#endif
// END defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)