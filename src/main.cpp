#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <DS3231.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <LiquidMenu.h>
#include <time.h>

unsigned long timeSinceLastIP;

const unsigned long sleepTime = 10000; //sleeep time in ms

const int EEPROM_intervalTime = 0x00;
const int EEPROM_wateringDuration = 0x01; //stores the value of watering duration in SECONDS

const bool pullup = true;

const int PIN_INT_Button = 3;
const int PIN_INT_RTC = 2;
const int PIN_FloatSwitch = A4;
const int PIN_LCD_Backlight = 10;
const int PIN_Pump = 12;
const int PIN_Solenoid = 13;

int intervalTime = 4;
volatile int wateringDuration = 1; //in MINUTES
float batteryVoltage = 0;

const int pinUpButton = A3;
const int pinDownButton = A2;
const int pinLeftButton = A1;
const int pinRightButton = A0;

const int tempButton = A5;

const int pinFaultLED = 11;

bool alarmFlag = false;

LiquidLine LINE1_OSF(0, 0, "Backup Battery");
LiquidLine LINE2_OSF(0, 1, "Change Battery");
LiquidScreen SCREEN_OSF(LINE1_OSF, LINE2_OSF);

LiquidLine LINE1_alarm_1(0,0, "Interval");
LiquidLine LINE2_alarm_1(0,1, intervalTime,"h");
LiquidScreen SCREEN_interval_time(LINE1_alarm_1, LINE2_alarm_1);

LiquidLine LINE1_Duration(0,0, "Duration");
LiquidLine LINE2_Duration(0,1, wateringDuration, " minutes");
LiquidScreen SCREEN_Duration(LINE1_Duration, LINE2_Duration);

LiquidLine LINE1_TestScreen(0,0, "Test pump");
LiquidLine LINE2_TestScreen(0,1, wateringDuration, " minutes");
LiquidScreen SCREEN_Test(LINE1_TestScreen, LINE2_TestScreen);

LiquidLine LINE1_Battery(0,0, "Battery");
LiquidLine LINE2_Battery(0,1, batteryVoltage,"V");
LiquidScreen SCREEN_Battery(LINE1_Battery, LINE2_Battery);

LiquidLine LINE1_WaterLow(0,0, "Water Low");
LiquidLine LINE2_WaterLow(0,1, "Refill tank");
LiquidScreen SCREEN_WaterLow(LINE1_WaterLow, LINE2_WaterLow);

LiquidCrystal lcd(4, 5, 6, 7, 8, 9);
DS3231 myRTC;

LiquidMenu mainMenu(lcd, SCREEN_Test, SCREEN_Duration, SCREEN_interval_time, SCREEN_Battery);
LiquidMenu OSFmenu(lcd, SCREEN_OSF);
LiquidMenu waterLowMenu(lcd, SCREEN_WaterLow);

LiquidSystem menu_system(mainMenu, OSFmenu, waterLowMenu);

void checkInputs(void);

void updateEEPROM(void);

void sleepNow(void);

void readEEPROMvalues(void);

void setNextAlarm(void);

void printTime(void);

void resetTime();

void printAlarmTime(void);

void waterThePlants(void);

void alarmTriggered(void);

void setup() {
  sleep_enable();
  Serial.begin(9600);
  Wire.begin();
  lcd.begin(16, 2);

  myRTC.setClockMode(false); //set ds3231 to 24 hour mode
  myRTC.turnOnAlarm(1);

  myRTC.setA1Time(0, 4, 0, 0, 0b00001000, false, false, false); //set alarm bits to 0b00001000, to trigger alarm every when hours match

  if ((EEPROM.read(EEPROM_intervalTime)!= 0xff) && (EEPROM.read(EEPROM_wateringDuration) != 0xff)){ //check to make sure EEPROM has been written to before (default value is 0xff)
    readEEPROMvalues();
  }

  pinMode(pinFaultLED, OUTPUT);
  digitalWrite(pinFaultLED, LOW);
  pinMode(PIN_LCD_Backlight, OUTPUT);
  pinMode(tempButton, INPUT_PULLUP);
  pinMode(PIN_Solenoid, OUTPUT);
  pinMode(PIN_Pump, OUTPUT);

  mainMenu.update();

  attachInterrupt(digitalPinToInterrupt(PIN_INT_Button), checkInputs, FALLING); //attach interrupt to button press
  attachInterrupt(digitalPinToInterrupt(PIN_INT_RTC), alarmTriggered, FALLING);

  delay(1000);


  if (!myRTC.oscillatorCheck()){

    Serial.println("OSF flag LOW - Time inaccurate!");

    digitalWrite(PIN_LCD_Backlight, HIGH);
    detachInterrupt(digitalPinToInterrupt(PIN_INT_Button));
    menu_system.change_menu(OSFmenu);
    while(true && digitalRead(PIN_INT_Button)){
      delay(249);
      digitalWrite(pinFaultLED, HIGH);
      delay(249);
      digitalWrite(pinFaultLED, LOW);

    }
    myRTC.setSecond(0);
    menu_system.change_menu(mainMenu);
    attachInterrupt(digitalPinToInterrupt(PIN_INT_Button), checkInputs, FALLING);

  }
  else {
    Serial.println("OSF flag High - Time good!");
  }

}

void loop() {
  if (millis() < timeSinceLastIP){ //if milis timer has ticked back to zero
    timeSinceLastIP = millis(); //act as if there has just been an input
  }

  if (digitalRead(tempButton) == LOW){
    Serial.println("Temp button pressed");
    resetTime();
  }

  printTime();
  printAlarmTime();
  alarmFlag = myRTC.checkIfAlarm(1); //for some reason i have to check the alarm this way, it won't work if i put this function in an if statement?!?!?!?!?!?!

  delay(500); //just a delay so the serial monitor doesn't get spammed

  if(alarmFlag){//check if the alarm flag is high, if it is, it will reset it and then call the water the plants fucntion
    Serial.println("Alarm 1 triggered");
    waterThePlants(); //uncomment when done testing
    setNextAlarm();
  }
  else {
    Serial.println("Alarm 1 not triggered");
  }


  if (millis() > timeSinceLastIP + sleepTime){
    updateEEPROM();
    setNextAlarm();
    delay(100); //delay just to allow stuff to happen / finish before going to sleep
    sleepNow();
  }
  

}

void checkInputs(void){

  Serial.println("Button press detected");

  timeSinceLastIP = millis();
  if(!digitalRead(PIN_LCD_Backlight)){
    digitalWrite(PIN_LCD_Backlight, HIGH);
    return;
  }

  if (digitalRead(pinLeftButton) == LOW){
    Serial.println("Left button pressed");
    mainMenu.previous_screen();
  }

  if (digitalRead(pinRightButton) == LOW) {
    Serial.println("Right button pressed");
    mainMenu.next_screen();
  }

  if (digitalRead(pinUpButton) == LOW){
    Serial.println("Up button pressed");

    if (mainMenu.get_currentScreen() == &SCREEN_Test ||mainMenu.get_currentScreen() == &SCREEN_Duration){
      if (wateringDuration >= 0 && wateringDuration <6){
        wateringDuration++;
      }
    }

    if (mainMenu.get_currentScreen() == &SCREEN_interval_time){
      if (intervalTime == 4){
        intervalTime = 6;
      }
      else if (intervalTime == 6){
        intervalTime = 12;
      }
      else {
       intervalTime = 4;
      }
    }

  }

  if (digitalRead(pinDownButton) == LOW){
    Serial.println("Down button pressed");

    if (mainMenu.get_currentScreen() == &SCREEN_Test || mainMenu.get_currentScreen() == &SCREEN_Duration){
      if (wateringDuration > 0 && wateringDuration <=6){
        wateringDuration--;
      }
    }

  }

  mainMenu.update();

}

void updateEEPROM(void){
  EEPROM.write(EEPROM_intervalTime, intervalTime);
  EEPROM.write(EEPROM_wateringDuration, wateringDuration);

  Serial.println("EEPROM Values written:");
  Serial.print("Interval Time: ");
  Serial.println(EEPROM.read(EEPROM_intervalTime));
  Serial.print("Watering Duration: ");
  Serial.println(EEPROM.read(EEPROM_wateringDuration));

}

void sleepNow(void){
  Serial.println("Going to sleep");
  delay(100); //delay to allow serial to finish sending
  digitalWrite(PIN_LCD_Backlight, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();
  delay(500);
}

void readEEPROMvalues(void){
  intervalTime = EEPROM.read(EEPROM_intervalTime);
  wateringDuration = EEPROM.read(EEPROM_wateringDuration);

  Serial.println("Interval time and watering duration global variables updated from EEPROM");
  Serial.print("Interval Time: ");
  Serial.println(intervalTime);
  Serial.print("Watering Duration: ");
  Serial.println(wateringDuration);
  Serial.println("EEPROM Values Successfully read");
}

void setNextAlarm(void){

  //reset time to 0
  myRTC.setSecond(0);
  myRTC.setMinute(0);
  myRTC.setHour(0);
  myRTC.checkIfAlarm(1);//clear alarm flag - should already be done but you know
  myRTC.turnOnAlarm(1);//again, shouldnt be needed but just in case

  myRTC.setA1Time(0, intervalTime, 0, 0, 0b00001000, false, false, false); //alarm bits to alarm when hours, mins and seconds match 
{
  byte second, minute, hour, day, bits;
  bool dy, h12, pm;

  myRTC.getA1Time(day, hour, minute, second, bits, dy, h12, pm);

  Serial.print("Next alarm set for: ");
  Serial.print(hour);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.print(second);
  Serial.print(" : ");
  Serial.println(day);
  
}

}

void resetTime(){//for testing purposes, reset the time to 5s before the alarm triggers (on 4h mode)
  myRTC.checkIfAlarm(1);
  myRTC.setSecond(55);
  myRTC.setMinute(59);
  myRTC.setHour(intervalTime - 1);
  Serial.println("Time reset to intervaltime-1:59:55");
}

void printTime(void){
  int second, minute, hour;
  bool h12, pm;

  second = myRTC.getSecond();
  minute = myRTC.getMinute();
  hour = myRTC.getHour(h12, pm);

  Serial.print(second);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.println(hour);
}

void printAlarmTime(void){
  byte second, minute, hour, day, bits;
  bool dy, h12, pm;

  myRTC.getA1Time(day, hour, minute, second, bits, dy, h12, pm);

  Serial.print("Alarm 1:    ");
  Serial.print(second);
  Serial.print(":");
  Serial.print(minute);
  Serial.print(":");
  Serial.println(hour);
}

void waterThePlants(void){
  //Turns on the pump and solenoid for the duration specified (wateringDuration*1000
  //and has a delay of 1 second between turning on the pump and solenoid
  //to prevent the pump from starting before the solenoid is fully open
  unsigned long wateringTimeInMinutes = (EEPROM.read(EEPROM_wateringDuration) * 60000); //must be cast to unsinged long as an int will overflow :(
  Serial.println("Watering plants");
  digitalWrite(PIN_Pump, HIGH);
  delay(1000);
  digitalWrite(PIN_Solenoid, HIGH);
  Serial.println(wateringTimeInMinutes);
  delay(wateringTimeInMinutes);//watering duration in minuntes (variable is ms, *1000 for s, *60 for minutes)
  digitalWrite(PIN_Pump, LOW);
  delay(1000);
  digitalWrite(PIN_Solenoid, LOW);
  Serial.println("Plants watered");
}

void alarmTriggered(void){
  //do nothing, just here to wake up the arduino 
}