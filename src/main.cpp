#include <Arduino.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <DS3231.h>
#include <EEPROM.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <LiquidMenu.h>


unsigned long timeSinceLastIP;

const unsigned long sleepTime = 3000; //sleeep time in ms

const int EEPROM_intervalTime = 0x00;
const int EEPROM_wateringDuration = 0x01;

const bool pullup = true;

const int PIN_INT_Button = 3;
const int PIN_INT_RTC = 2;
const int PIN_FloatSwitch = A4;
const int PIN_LCD_Backlight = 10;
const int PIN_Pump = 12;
const int PIN_Solenoid = 13;

int intervalTime = 4;
int wateringDuration = 0;
float batteryVoltage = 0;

const int pinUpButton = A3;
const int pinDownButton = A2;
const int pinLeftButton = A1;
const int pinRightButton = A0;

const int pinFaultLED = 11;

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

LiquidCrystal lcd(4, 5, 6, 7, 8, 9);
DS3231 myRTC;

LiquidMenu mainMenu(lcd, SCREEN_Test, SCREEN_Duration, SCREEN_interval_time, SCREEN_Battery);
LiquidMenu OSFmenu(lcd, SCREEN_OSF);

LiquidSystem menu_system(mainMenu, OSFmenu);

void checkInputs(void);

void updateEEPROM(void);

void sleepNow(void);

void alarmTriggered(void);

void setup() {
  // put your setup code here, to run once:
  sleep_enable();

  pinMode(pinFaultLED, OUTPUT);
  digitalWrite(pinFaultLED, LOW);
  pinMode(PIN_LCD_Backlight, OUTPUT);

  lcd.begin(16, 2);
  Serial.begin(9600);
  Wire.begin();
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
  // put your main code here, to run repeatedly:
  if (millis() < timeSinceLastIP){ //if milis timer has ticked back to zero
    timeSinceLastIP = millis(); //act as if there has just been an input
  }

  if (millis() > timeSinceLastIP + sleepTime){
    updateEEPROM();
    delay(100);
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
  //EEPROM.write(EEPROM_intervalTime, intervalTime);
  //EEPROM.write(EEPROM_wateringDuration, wateringDuration);

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

void alarmTriggered(void){

  const bool floatSwitchLogicState = HIGH;

  if (digitalRead(PIN_FloatSwitch) == floatSwitchLogicState){ //change logic states depending on if float switch is active HIGH or LOW
    Serial.println("Float switch triggered");
    while (digitalRead(PIN_FloatSwitch) == floatSwitchLogicState){//infinite loop, waiting for water to rise again. 
      digitalWrite(pinFaultLED, HIGH);
      delay(100);
      digitalWrite(pinFaultLED, LOW);
      delay(100);
    }
    delay(30000); //30 second delay to avoid accidental pump activation when water is too low
    alarmTriggered();//go back and check the pump

    //i wonder if we will need something to detach the button interrupts, to avoid exiting this loop

  }

  /*
  open solenoid
  turn on pump
  delay for watering duration
  turn off pump
  close solenoid
  set the alarm for the next watering time
    something to calculate the next watering time - next time = current time+interval time????
  clear alarm flag? something like that anyway 
  go back to sleep
  just adding some text to test git
  */

}