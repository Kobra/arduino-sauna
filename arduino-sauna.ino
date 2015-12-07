byte hiTemp = 120;
bool hiTempAlarm = false;
byte tempStep = 2;
float lastTemp;
int tempDir;
bool cool = false;
bool CD = false;
bool firstCD = false;
bool secondCD = false;
uint32_t cdTimestamp = 0;
bool adminState = false;
uint32_t offTimestamp = 0;

//Store values in EEPROM
#include <EEPROM.h>
byte hiTempAddr = 0;

//LCD
#include <LiquidCrystal.h>
#include <LCDKeypad.h>
LCDKeypad lcd;
byte BLPin = 10;
byte grad[8] = {
  B01000,
  B10100,
  B01000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000
};
String line1 = "";
String line2 = "";
String stateMsg;
byte len, s;

//Temperature Sensor
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 12
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
//DeviceAddress SensorAddr = { 0x28, 0xFF, 0x55, 0x8E, 0x03, 0x15, 0x02, 0x88 };
DeviceAddress SensorAddr;

//Analog button
byte buttonPin = 13;

//RTC
#include <Wire.h>
#include <RTClib.h>
RTC_DS1307 rtc;
uint32_t lastTimestamp;

//Relay
byte relayPin = 2;

//Beep
byte beepPin = 3;

void beep(int duration = 100, byte level = 20) {
  analogWrite(beepPin, level);
  delay(duration);
  analogWrite(beepPin, 0);
}

void playTone(int tone, int duration) {
  for (long i = 0; i < duration * 1000L; i += tone * 2) {
    digitalWrite(beepPin, HIGH);
    delayMicroseconds(tone);
    digitalWrite(beepPin, LOW);
    delayMicroseconds(tone);
  }
}

void alarm() {
  playTone(1700,500);
  playTone(956,500);
  playTone(1700,500);
  playTone(956,500);
}

void checkTemp(float temp) {
  if(adminState) {
    byte checkTemp = hiTemp;
    if(lastTemp > temp) tempDir=-1;
    if(lastTemp == temp) tempDir=0;
    if(lastTemp < temp) tempDir=1;
    lastTemp = temp;
    //Serial.print("Current Temperature: ");
    //Serial.println(temp);

    if(!hiTempAlarm && temp >= hiTemp) {
      hiTempAlarm = true;
      alarm();
      alarm();
    }
    
    if(cool) checkTemp = hiTemp - tempStep;

    if(CD) {
      checkTemp=0;
      if(cdTimestamp == 0) cdTimestamp = lastTimestamp + 300;
      if(lastTimestamp >= cdTimestamp) {CD=false;cdTimestamp=0;}
    }

    if(!CD && !firstCD && temp == 45) {CD=true;firstCD=true;}
    if(!CD && !secondCD && temp == 65) {CD=true;secondCD=true;}
  
    //Serial.print("Current State: ");
    if(temp <= checkTemp) {
      relay(true);
      //Serial.println("Heating");
    } else {
      relay(false);
      //Serial.println("Cooling");
    }
    //Serial.print("Temperature direction: ");
    //if(tempDir == 1) Serial.println("Up");
    //if(tempDir == 0) Serial.println("Same");
    //if(tempDir == -1) Serial.println("Down");
    
    //Serial.print("Max Temperature: ");
    //Serial.print(hiTemp);
    //Serial.print(" Cooling step: ");
    //Serial.println(tempStep);
  } else {
    relay(false);
    firstCD = false;
    secondCD = false;
    CD = false;
    hiTempAlarm = false;
  }
}

void backlight(byte level) {
  analogWrite(BLPin, level);
}

float getTemp() {
  if(sensors.isConnected(SensorAddr)) {
    sensors.requestTemperaturesByAddress(SensorAddr);
    float tempC = sensors.getTempC(SensorAddr);
    return tempC;
  } else {
    return false;
  }
}

void printLines() {
  len=line1.length(); for(s=0; s<16-len; s++) line1+=" ";
  len=line2.length(); for(s=0; s<16-len; s++) line2+=" ";
  lcd.setCursor(0,0);
  lcd.print(line1);
  lcd.setCursor(0,1);
  lcd.print(line2);
  Serial.println(line1);
  Serial.println(line2);
}

void relay(bool state) {
  cool = !state;
  if(state) digitalWrite(relayPin, HIGH);
  else digitalWrite(relayPin, LOW);
}

void setup() {
  Serial.begin(9600);
  //#ifndef ESP8266
  //  while (!Serial); // for debug console Leonardo/Micro/Zero
  //#endif
  lcd.begin(16, 2);
  lcd.createChar(0, grad);
  pinMode(BLPin, OUTPUT);
  backlight(12);

  if (!sensors.getAddress(SensorAddr, 0)) Serial.println("Unable to find address for Device 0"); 
  sensors.setResolution(SensorAddr, 9);

  hiTemp = EEPROM.read(hiTempAddr);
  if(hiTemp < 20 || hiTemp > 120) {
    hiTemp = 85;
    EEPROM.write(hiTempAddr,hiTemp);
  }

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    //while (1);
  }
  //Set Date and Time to compile time
  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  if (!rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(buttonPin, INPUT);
  pinMode(relayPin, OUTPUT);
  relay(false);
  pinMode(beepPin, OUTPUT);
  beep();
}

int readButtons() {
  //R - 1; U - 2; D - 3; L - 4; S - 5; Analog - 10;
  int buttonPressed=lcd.button();
  if(buttonPressed != KEYPAD_NONE) {waitReleaseButton(); buttonPressed++;}
  else buttonPressed=false;
  
  if(digitalRead(buttonPin) == HIGH) {
    while(!digitalRead(buttonPin) == LOW) delay(10);
    buttonPressed=10;
  }
  
  if(buttonPressed) beep();
  return buttonPressed;
}

void waitReleaseButton() {
  delay(50);
  while(lcd.button()!=KEYPAD_NONE) {}
  delay(50);
}

void loop() {
  int pressedButton = readButtons();
  //R - 1; U - 2; D - 3; L - 4; S - 5; Analog - 10;
  if(pressedButton) {
    byte tmpStep;
    if(pressedButton == 10) adminState = !adminState;
    if(pressedButton == 2) { //Up
      if(hiTemp < 70) tmpStep = 10;
      else tmpStep = 5;
      hiTemp += tmpStep;
      if(hiTemp > 120) {beep(200);hiTemp=120;}
      EEPROM.write(hiTempAddr,hiTemp);
    }
    if(pressedButton == 3) { //Down
      if(hiTemp <= 70) tmpStep = 10;
      else tmpStep = 5;
      hiTemp -= tmpStep;
      if(hiTemp < 20) {beep(200);hiTemp=20;}
      EEPROM.write(hiTempAddr,hiTemp);
    }
  }
  DateTime now = rtc.now();
  if(rtc.isrunning()) {
    if(now.unixtime() <= lastTimestamp) return;
    lastTimestamp = now.unixtime();
  } else {
    delay(1000);
    rtc.begin();
    lastTimestamp = 0;
  }
  if(adminState) {
    backlight(255);
    if(offTimestamp == 0) offTimestamp = lastTimestamp + 21600;
    else if(lastTimestamp >= offTimestamp) adminState = false;
  }
  else {
    backlight(12);
    offTimestamp = 0;
  }
  line1 = ""; line2 = "";
  float temp = getTemp();
  if(temp) {
    checkTemp(temp);
    line2 += "t : ";
    line2 += int(temp);
    len=line2.length(); for(s=0; s<13-len; s++) line2+=" ";
    if(hiTemp<100) line2+=" ";
    if(hiTemp<10) line2+=" ";
    line2 += hiTemp;
  } else {
    relay(false);
    line2 += "Sensor error!";
  }
  if(cool) stateMsg = "  Cool";
  else stateMsg = "  Heat";
  if(CD) {
    int CDcount = cdTimestamp - lastTimestamp;
    stateMsg = "CD "; stateMsg += CDcount;
  }
  if(adminState) {
    line1 += "On ";
    line1 += stateMsg;
  }
  else line1 += "Off";
  if(rtc.isrunning()) {
    String clockSep;
    if(now.second() & 1) clockSep = ":"; else clockSep = " ";
    len=line1.length(); for(s=0; s<11-len; s++) line1+=" ";
    if(now.hour()<10) line1+="0"; line1+=now.hour();
    line1 += clockSep;
    if(now.minute()<10) line1+="0"; line1 += now.minute();
  }
  printLines();
  if(temp) {lcd.setCursor(1,1); lcd.write(byte(0));}
}

