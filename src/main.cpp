#include <Arduino.h>
#include <SPI.h> // https://community.platformio.org/t/adafruit-busio-adafruit-spidevice-h17-fatal-error-spi-h-no-such-file-or-directory/14864/9
#include "DCF77.h"
#include "Time.h"
#include <Timezone.h>
#include "RTClib.h"
#include <avr/sleep.h>  // Include the AVR sleep library
#include <avr/power.h>  // Optional, if you want to disable/enable peripherals

#define DCF_PIN 2	         // Connection pin to DCF 77 device
#define DCF_INTERRUPT 0		 // Interrupt number associated with pin
#define lightPin A0        // photo resistor sensor
#define keyInput A1        // input from resistors' keyboard
#define PRESS_TOLERANCE 40
#define PRESSED_TIME 10
#define LED1      5
#define LED2      6
#define pirPin    3
#define STAYON   120000UL  // 10 min in milliseconds
#define DELTA    10


#include "fidelio_display.h"

#ifdef FIDELIODISPLAY_h

    #define dioPin 13
    #define clkPin 14

    #define stbPin 10
    #define spiClk 250000UL 

    FidelioDisplay display(dioPin, clkPin, stbPin, spiClk);

#endif

char timetxt[5];

// more time zones, see  http://en.wikipedia.org/wiki/Time_zones_of_Europe
// United Kingdom (London, Belfast)
// TimeChangeRule rBST = {"BST", Last, Sun, Mar, 1, 60};   //British Summer Time
// TimeChangeRule rGMT = {"GMT", Last, Sun, Oct, 2, 0};    //Standard Time
// Timezone UK(rBST, rGMT);

TimeChangeRule rCEST = {"CEST", Last, Sun, Mar, 2, 120};   // starts last Sunday in March at 2:00 am, UTC offset +120 minutes; Central European Summer Time (CEST)
TimeChangeRule rCET =  {"CET", Last, Sun, Oct, 3, 60};     // ends last Sunday in October at 3:00 am, UTC offset +60 minutes; Central European Time (CET)
Timezone CET(rCEST, rCET);


time_t time;
DCF77 DCF = DCF77(DCF_PIN,DCF_INTERRUPT);
RTC_DS1307 rtc;

unsigned long getDCFTime()
{ 
  time_t DCFtime = DCF.getUTCTime(); // Convert from UTC
  
  if (DCFtime!=0) {
    return DCFtime;
  }
  return 0;
}

void printDigits(int digits){
  // utility function for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

void intToTimeString(char *str, int hour, int minute) {
    // Ensure hours and minutes are within 0-99
    hour = hour % 100;
    minute = minute % 100;

    // Convert each digit to a character and place it in the string
    str[0] = '0' + hour / 10;
    str[1] = '0' + hour % 10;
    str[2] = '0' + minute / 10;
    str[3] = '0' + minute % 10;
    str[4] = '\0'; // Null-terminate the string
}

void showSyncProcess(){
  #ifdef FIDELIODISPLAY_h
    intToTimeString(timetxt, 0, DCF.bufLen());
    timetxt[0] = '0'+DCF.lastBit;
    timetxt[1] = ':';
    display.pm(!DCF.bufOk);
    display.write(timetxt);
    // Serial.println(); Serial.print("In sync process: "); Serial.println(timetxt);
  #endif
}

time_t dateTimeToTime_t(DateTime dt) {
    tmElements_t tm;
    tm.Year = dt.year() - 1970;
    tm.Month = dt.month();
    tm.Day = dt.day();
    tm.Hour = dt.hour();
    tm.Minute = dt.minute();
    tm.Second = dt.second();
    return makeTime(tm); // Convert to time_t
}

DateTime time_tToDateTime(time_t t) {
    tmElements_t tm;
    breakTime(t, tm); // Break the time_t into components

    // Construct and return a DateTime object
    return DateTime(tm.Year + 1970, tm.Month, tm.Day, tm.Hour, tm.Minute, tm.Second);
}


void wakeUp() {
}

void goToSleep() {
  // Simulated sleep mode
  ADCSRA &= ~(1 << ADEN);  // Disable ADC to save power
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();

  // Processor wakes up here after ISR
  sleep_disable();
  ADCSRA |= (1 << ADEN);   // Re-enable ADC
  setTime(dateTimeToTime_t(rtc.now()));
  DCF.Start();
}

time_t prevDisplay = 0;          // when the digital clock was displayed
time_t lastSync = 0;
timeStatus_t lastStatus = timeNotSet;


enum clockStatusT {main, showDCF, other};
// Define button values
const int buttonValues[5] = {0, 359, 654, 765, 1000}; // Added a fifth value for easier loop checking
int myButton();
int myButton() {
  static int lastButtonState = 0; // last stable button state
  static unsigned long lastDebounceTime = 0; // the last time the output pin was toggled
  static bool buttonNotHandled = true;

  unsigned long currentTime = millis();
  int buttons = analogRead(keyInput);
  // Check each button
  char i = 0;
  do {
    if (buttons < buttonValues[i] + PRESS_TOLERANCE && buttons > buttonValues[i] - PRESS_TOLERANCE) {
      break;
    }
    i++;
  } while ( i < 4); // only 4 buttons 0,1,2,3
  if (i > 3) { // no pressed buttons detected index > number of buttons value (counting from 0: 0,1,2,3)
    lastButtonState = 0;      // no pressed buttons detected
    buttonNotHandled = true;  // we haven't handled any buttons yet
    return 0;
  }
  if (lastButtonState != i+1) { // detected state change
    lastDebounceTime = currentTime; // start debounce loop
    lastButtonState = i+1; // memorize curent button state
  }
  if (currentTime - lastDebounceTime > PRESSED_TIME) {
    if (lastButtonState == i+1 && buttonNotHandled) {
      // Serial.println(buttons);
      buttonNotHandled = false;
      return lastButtonState;
    } else return 0;

  } else return 0;
}

void setup() {
  Serial.begin(9600);
  pinMode(LED1, OUTPUT);
  pinMode(LED2, OUTPUT);
  digitalWrite(LED1, 0);
  digitalWrite(LED2, 0);
  delay(1000);

  pinMode(pirPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(pirPin), wakeUp, RISING);
  
  DCF.Start();
  setSyncProvider(DCF.getUTCTime);

  #ifdef FIDELIODISPLAY_h
    display.init();
    display.cls();
    display.setBright(7);
  #endif

  if (! rtc.begin()) {
    Serial.println("Could not find RTC");
  }

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");

    setSyncInterval(30);

    Serial.println("Waiting for DCF77 time ... ");
    Serial.println("It will take at least 2 minutes until a first update can be processed.");
    while(timeStatus()== timeNotSet) { 
      showSyncProcess();
      delay(250);
    }
    rtc.adjust(time_tToDateTime(now()));
    Serial.println("Updated ATmega to DCF") ;
  } else {
    setTime(dateTimeToTime_t(rtc.now()));
    Serial.println("Updated ATmega to RTC") ;
  }
  setSyncInterval(180);
  lastStatus = timeStatus();
  lastSync = now();

}

void loop() {

  long timeON;
  static unsigned long lastMovementTime = 0;
  static int lastPIRState = -1;
  static int lastBrightness = -1;
  static bool sleepON = false;
  static clockStatusT clockStatus = main;

  static int fidelioBrightness = 7;

  int currentPIRState = digitalRead(pirPin);
  if (currentPIRState != lastPIRState) {
    lastPIRState = currentPIRState;
  }
  if (currentPIRState == HIGH) {
    lastMovementTime = millis();
  }

  #define maxLight 300
  int currentBrightness = analogRead(lightPin);
  currentBrightness = constrain(currentBrightness, 0, maxLight);
  if (abs(currentBrightness - lastBrightness) >= DELTA) {
    lastBrightness = currentBrightness;
    fidelioBrightness = map(maxLight-currentBrightness, 0, maxLight, 0, 8);
  }
  
  int button = myButton();
  if (button > 0) {
    // Serial.println(); Serial.print("Pressed: ");     Serial.println(button);
    // digitalWrite(LED_BUILTIN, (digitalRead(LED_BUILTIN) ^ 1));
    // if (1 == button || 2 == button || 3 == button || 4 == button) {
    //   digitalWrite(LED1, (digitalRead(LED1) ^ 1));
    //   // analogWrite(LED1, analogRead(A0)/4);
    // } // else analogWrite(LED1, 0);
    if (2 == button) {
      clockStatus = (clockStatus == showDCF ? main:showDCF);
    }
  }

  switch (clockStatus)  {
      case main:
        if(now() != prevDisplay) //update the display only if the time has changed
        {
          prevDisplay = now();
          // digitalClockDisplay();
          time_t LocalTime = CET.toLocal(now());
          intToTimeString(timetxt, hour(LocalTime), minute(LocalTime));

          if (!sleepON)
            display.setBright(fidelioBrightness);
            // Serial.println(); Serial.print("Level: "); Serial.println(fidelioBrightness);
            // Serial.print("TS:");
            // Serial.println(timeStatus());
          display.alarm( (timeStatus() != timeSet) );
          display.pm(!DCF.bufOk);
          display.toogleDots(); 
          display.write(timetxt);
          int delta = now() - dateTimeToTime_t(rtc.now());
          if ( 0 != delta ) {
            if (timeStatus() == timeSet && DCF.bufOk) {
              rtc.adjust(time_tToDateTime(now()));
              Serial.println();
              Serial.print("Updated RTC to DCF77 by: ") ;
              Serial.println(delta);
            } else {
              setTime(dateTimeToTime_t(rtc.now()));
              Serial.println();
              Serial.print("Updated ATmega to RTC by: ") ;        
              Serial.println(delta) ;
            }
          }
        }
        timeON = abs(millis() - lastMovementTime);
        if (!currentPIRState && timeON  > STAYON) {
          sleepON = true;    
          Serial.print("Going Sleep");
          display.Off();
          DCF.Stop();
          delay(100);
          goToSleep();
        } else sleepON = false;
        // Serial.println(); Serial.print("Status 0: "); Serial.println(clockStatus);
        break;
      case showDCF:
        //Serial.println(); Serial.print("Status 1: "); Serial.println(clockStatus);
        display.setBright(fidelioBrightness);
        showSyncProcess();
        if (timeStatus() == timeSet && DCF.bufOk) { 
          rtc.adjust(time_tToDateTime(now()));
          Serial.println("Time updated to DCF");
          clockStatus = main;
        }
        break;
      default:
        break;
  }
}

