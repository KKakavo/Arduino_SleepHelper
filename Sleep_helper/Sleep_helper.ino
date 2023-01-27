#include <LiquidCrystal_I2C.h>
#include<Adafruit_Sensor.h>
#include <DHT.h>
#include<MQ135.h>
#include<FastLED.h>
#include<TroykaLight.h>
#include<RTClib.h>
#include<EEPROM.h>
#include<pitches.h>
//Пины 
#define DHTPIN 3
#define MQPIN A0
#define LEDPIN 5
#define LPIN A2
#define POTPIN A1
#define BUTPIN 2
#define PIEZOPIN 6
//RTC SDA = A4 SCL = A5
//ID знаков
#define T_ID 1
#define S_ID 2
#define L_ID 3
#define CD_ID 4
#define H_ID 5
#define GC_ID 6
#define PPM_ID 7
#define DB_ID 8

#define LED_COUNT 4
#define TEMPERATURE_LIMIT_TOP 20
#define TEMPERATURE_LIMIT_BOT 15
#define LIGHT_LIMIT 20
#define CARBON_DIOXIDE_LIMIT 1000
#define HUMIDITY_LIMIT_BOT 40
#define HUMIDITY_LIMIT_TOP 60
#define POTMIN 90
#define POTMAX 950
#define EEPROMAWAKEPOS 0
#define EEPROMSLEEPPOS 4

#define MENU_SCENE 0
#define SLEEP_SCENE 1
#define AWAKE_SCENE 2
//Инициализация ЖК дисплея и датчиков
DHT dht(DHTPIN, DHT11);
TroykaLight sensorLight(LPIN);
LiquidCrystal_I2C LCD(0x3F, 16, 2);  // присваиваем имя LCD для дисплея
RTC_DS1307 rtc;
MQ135 MQ = MQ135(MQPIN);
CRGB strip[LED_COUNT];

//Создание значков
byte temperatureChar[8] = {
  0b00000,
  0b01010,
  0b11100,
  0b01000,
  0b01000,
  0b01010,
  0b00100,
  0b00000
};
byte noiseChar[8] = {
  0b00000,
  0b00001,
  0b01010,
  0b11000,
  0b11011,
  0b11000,
  0b01010,
  0b00001
};
byte lightChar[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b11011,
  0b10101,
  0b11011,
  0b01110,
  0b00000
};
byte carbDioxChar[8] = {
  0b11011,
  0b10011,
  0b10011,
  0b11011,
  0b00000,
  0b01010,
  0b01010,
  0b01010
};
byte humidityChar[8] = {
  0b00000,
  0b00000,
  0b01110,
  0b01010,
  0b10001,
  0b10011,
  0b10101,
  0b01110
};
byte celsDegreesChar[8] = {
  B11000,
  B11000,
  B00011,
  B00100,
  B00100,
  B00100,
  B00100,
  B00011
};
byte ppmChar[8] = {
  B11011,
  B11011,
  B10010,
  B00000,
  B10001,
  B11011,
  B10101,
  B10001
};
int notesAwake[] = {
  392, 392, 392, 311, 466, 392, 311, 466, 392,
  587, 587, 587, 622, 466, 369, 311, 466, 392,
  784, 392, 392, 784, 739, 698, 659, 622, 659,
  415, 554, 523, 493, 466, 440, 466,
  311, 369, 311, 466, 392
};
int timesAwake[] = {
  350, 350, 350, 250, 100, 350, 250, 100, 700,
  350, 350, 350, 250, 100, 350, 250, 100, 700,
  350, 250, 100, 350, 250, 100, 100, 100, 450,
  150, 350, 250, 100, 100, 100, 450,
  150, 350, 250, 100, 750
};
int noteSleep[] = {
  NOTE_D4, 0, NOTE_F4, NOTE_D4, 0, NOTE_D4, NOTE_G4, NOTE_D4, NOTE_C4,
  NOTE_D4, 0, NOTE_A4, NOTE_D4, 0, NOTE_D4, NOTE_AS4, NOTE_A4, NOTE_F4,
  NOTE_D4, NOTE_A4, NOTE_D5, NOTE_D4, NOTE_C4, 0, NOTE_C4, NOTE_A3, NOTE_E4, NOTE_D4,
  0,NOTE_D4,NOTE_D4
};
int timesSleep[] = {
  8, 8, 6, 16, 16, 16, 8, 8, 8, 
  8, 8, 6, 16, 16, 16, 8, 8, 8,
  8, 8, 8, 16, 16, 16, 16, 8, 8, 2,
  8,4,4
};

byte sleepTime[2];
byte awakeTime[2];
  //Инициализация
void setup(){
  LCD.init();
  Serial.begin(9600);
  dht.begin();
  FastLED.addLeds<WS2812B, LEDPIN, GRB>(strip, LED_COUNT).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(255);
  pinMode(BUTPIN,INPUT_PULLUP);
  attachInterrupt(0,butPressed,FALLING);
  pinMode(MQPIN, INPUT);
  pinMode(POTPIN, INPUT);
  pinMode(PIEZOPIN,OUTPUT);
  LCD.createChar(T_ID, temperatureChar);
  LCD.createChar(S_ID, noiseChar);
  LCD.createChar(L_ID, lightChar);
  LCD.createChar(CD_ID, carbDioxChar);
  LCD.createChar(H_ID, humidityChar);
  LCD.createChar(GC_ID, celsDegreesChar);
  LCD.createChar(PPM_ID, ppmChar);
  LCD.backlight();  // включение подсветки дисплея
   if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
  }
    rtc.adjust(DateTime(__DATE__, __TIME__));
    changeScene(MENU_SCENE);
    EEPROM.get(EEPROMAWAKEPOS,awakeTime);
    EEPROM.get(EEPROMSLEEPPOS,sleepTime);
}
bool tIsOn = true, hIsOn = true, lIsOn = true, cdIsOn = true;
float temperature;
int humidity;
int carbDiox;
int light;
int potData;
int curScene = MENU_SCENE;
bool butFlag = false;
int time[2];
int timePos = 0;
volatile bool flag = true;
DateTime currentTime;

void loop() {
  getData();
  LEDwrite();
  log();
  timeToSW();
  showScene(curScene);
}


void getData() {
  currentTime = rtc.now();
  sensorLight.read();
  light = sensorLight.getLightLux();
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  carbDiox = MQ.getCorrectedPPM(temperature, humidity);
  potData = analogRead(POTPIN);
}

void LEDwrite() {
  if(temperature < TEMPERATURE_LIMIT_BOT && tIsOn)
    for (int i = 0; i < LED_COUNT; i += 4)
      strip[i] = CRGB::DarkBlue;  // Синий цвет
  else if(temperature > TEMPERATURE_LIMIT_TOP)
    for (int i = 0; i < LED_COUNT; i += 4)
      strip[i] = CRGB::DarkRed;  // Красный цвет
  else
    for (int i = 0; i < LED_COUNT; i += 4)
     strip[i] = CRGB::Black;  // Выключение


  if(light > LIGHT_LIMIT && lIsOn)
    for (int i = 1; i < LED_COUNT; i += 4)
      strip[i] = CRGB::DarkViolet;  // Розовый цвет
  else
    for (int i = 1; i < LED_COUNT; i += 4)
      strip[i] = CRGB::Black;  // Выключение

  if(carbDiox > CARBON_DIOXIDE_LIMIT && cdIsOn)
    for (int i = 3; i < LED_COUNT; i += 4)
      strip[i] = CRGB::Green;  // Зелёный цвет.
  else
    for (int i = 3; i < LED_COUNT; i += 4)
      strip[i] = CRGB::Black;  // Выключение

  if(humidity > HUMIDITY_LIMIT_TOP && hIsOn)
    for (int i = 2; i < LED_COUNT; i += 4)
      strip[i] = CRGB::Teal;  // Бирюзовый цвет
  if(humidity < HUMIDITY_LIMIT_BOT && hIsOn)
    for (int i = 2; i < LED_COUNT; i += 4)
      strip[i] = CRGB::Orange;  // Жёлтый цвет
  else
    for (int i = 2; i < LED_COUNT; i += 4)
       strip[i] = CRGB::Black;  // Выключение
  FastLED.show();
}

void log() {
  Serial.print(currentTime.hour());
  Serial.print(":");
  Serial.print(currentTime.minute());
  Serial.print(":");
  Serial.println(currentTime.second());
  Serial.print("Awake time: ");
  Serial.print(awakeTime[0]);
  Serial.print(":");
  Serial.println(awakeTime[1]);
  Serial.print("Sleeptime: ");
  Serial.print(sleepTime[0]);
  Serial.print(":");
  Serial.println(sleepTime[1]);
  Serial.print("Carbon dioxide: ");
  Serial.print(carbDiox);
  Serial.println(" ppm");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println(" %");
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" *C ");
  Serial.print("Light: ");
  Serial.print(light);
  Serial.println(" Lx");
  Serial.print("POT: ");
  Serial.println(potData);
  Serial.println("**********\n\n");
}


void timeToSW(){
  if(currentTime.hour() == awakeTime[0] && currentTime.minute() == awakeTime[1] && currentTime.second() == 0){
    musicAwakeSetup();
    musicAwake();
    changeScene(MENU_SCENE);
  }
  else if(currentTime.hour() == sleepTime[0] && currentTime.minute() == sleepTime[1] && currentTime.second() == 0){
    musicSleepSetup();
    musicSleep();
    changeScene(MENU_SCENE);
  }

}
void musicAwakeSetup(){
  LCD.clear();
  LCD.noCursor();
  LCD.setCursor(0,0);
  LCD.print("Time to wake up!");
  flag = true;
}
void musicAwake(){
  while(flag == true){
  for(int i = 0; i < 39;i++){
    tone(PIEZOPIN, notesAwake[i], timesAwake[i]*2);
    delay(timesAwake[i]*2);
    noTone(PIEZOPIN);
    Serial.println(flag);
    if(flag == false)
      break;
  }
  }
  curScene = 0;
  flag = true;
}
 
void musicSleepSetup(){
  LCD.clear();
  LCD.noCursor();
  LCD.setCursor(0,0);
  LCD.print("Time to sleep!");
  flag = true;
}
void musicSleep(){
  while(flag == true){
  for(int i = 0; i < 31;i++){
    tone(PIEZOPIN, noteSleep[i], 1000 / timesSleep[i]);
    delay(1000/timesSleep[i]*1.8);
    noTone(PIEZOPIN);
    if(flag == false)
      break;
  }
  }
  curScene = 0;
  flag = true;
}

void showScene(int scene){
  if(scene == MENU_SCENE){
    dispShowMenu();
    dispClear();
    chooseIconMenu();
  }
  else if(scene == SLEEP_SCENE)
    dispShowSleep();
  else if(scene == AWAKE_SCENE)
    dispShowAwake();
}
void dispShowMenu() {
  String temperatureString;
  temperatureString.concat(temperature);
  temperatureString.remove(temperatureString.length() - 1);
  LCD.setCursor(1, 0);
  LCD.print(temperatureString);
  LCD.write(GC_ID);
  LCD.setCursor(9, 1);
  LCD.print(humidity);
  LCD.print("%");
  LCD.setCursor(1, 1);
  LCD.print(carbDiox);
  LCD.write(PPM_ID);
  LCD.setCursor(9, 0);
  LCD.print(light);
  LCD.print("Lx");
}
void dispClear() {
  String temperatureString;
  temperatureString.concat(temperature);
  temperatureString.remove(temperatureString.length() - 1);
  if (temperatureString.length() < 4) {
    LCD.setCursor(5, 0);
    LCD.print(" ");
  }
  if (humidity < 10) {
    LCD.setCursor(11, 1);
    LCD.print(" ");
  }
  for (int i = 2 + (log10(carbDiox) + 1); i <= 5; i++) {
    LCD.setCursor(i, 1);
    LCD.print(" ");
  }
  for(int i = 11 + (log10(light) + 1); i <=15;i++){
    LCD.setCursor(i, 0);
    LCD.print(" ");
  }
}
void chooseIconMenu(){
  int menuPos = map(potData,POTMIN,POTMAX,0,5);
  if(menuPos == 0)
    LCD.setCursor(0,0);
  else if(menuPos == 1)
    LCD.setCursor(8,0);
  else if(menuPos == 2)
    LCD.setCursor(0,1);
  else if(menuPos == 3)
    LCD.setCursor(8,1);
  else if(menuPos == 4)
    LCD.setCursor(13,1);
  else if(menuPos == 5)
    LCD.setCursor(15,1);
  
  if(!digitalRead(BUTPIN))
    butFlag = true;
  else if(butFlag == true){
    if(menuPos == 0)
      tIsOn = !tIsOn;
    else if(menuPos == 1)
      lIsOn = !lIsOn;
    else if(menuPos == 2)
      cdIsOn = !cdIsOn;
    else if(menuPos == 3)
      hIsOn = !hIsOn;
    else if(menuPos == 4){
      changeScene(SLEEP_SCENE);
    }
    else if(menuPos == 5){
      changeScene(AWAKE_SCENE);
    }
    butFlag = false;
  }
  Serial.println(menuPos);
}
void dispShowSleep(){
  if(timePos == 0){
    time[timePos] = map(potData,POTMIN,POTMAX,0,23);
    if(time[timePos] >=24)
      time[timePos] = 23;
    else if(time[timePos] < 0)
      time[timePos]=0;
  }
  else if(timePos == 1){
    time[timePos] = map(potData,POTMIN,POTMAX,0,59);
    if(time[timePos] >=60)
      time[timePos] = 59;
    else if(time[timePos] < 0)
      time[timePos]=0;
  }
  LCD.setCursor(3 * timePos,1);
  
  if(time[timePos]<10){
    LCD.print("0");
    LCD.print(time[timePos]);
  }
  else
    LCD.print(time[timePos]);
  if(!digitalRead(BUTPIN))
    butFlag = true;
  else if(butFlag == true){
    timePos++;
    if(timePos == 2){
      sleepTime[0] = time[0];
      sleepTime[1] = time[1];
      EEPROM.put(EEPROMSLEEPPOS,sleepTime);
      changeScene(MENU_SCENE);
    }
    butFlag = false;
  }
}
void dispShowAwake(){
  if(timePos == 0){
    time[timePos] = map(potData,POTMIN,POTMAX,0,23);
    if(time[timePos] >=24)
      time[timePos] = 23;
    else if(time[timePos] < 0)
      time[timePos]=0;
  }
  else if(timePos == 1){
    time[timePos] = map(potData,POTMIN,POTMAX,0,59);
    if(time[timePos] >=60)
      time[timePos] = 59;
    else if(time[timePos] < 0)
      time[timePos]=0;
  }
  LCD.setCursor(3 * timePos,1);
  
  if(time[timePos]<10){
    LCD.print("0");
    LCD.print(time[timePos]);
  }
  else
    LCD.print(time[timePos]);
  if(!digitalRead(BUTPIN))
    butFlag = true;
  else if(butFlag == true){
    timePos++;
    if(timePos == 2){
      awakeTime[0] = time[0];
      awakeTime[1] = time[1];
      EEPROM.put(EEPROMAWAKEPOS,awakeTime);
      changeScene(MENU_SCENE);
    }
    butFlag = false;
  }
}
void changeScene(int scene){
  if(scene == MENU_SCENE){
    curScene = MENU_SCENE;
    dispShowMenuSetup();
    timePos = 0;
  }
  else if(scene == SLEEP_SCENE){
    curScene = SLEEP_SCENE;
    dispShowSleepSetup();
  }
  else if(scene == AWAKE_SCENE){
    curScene = AWAKE_SCENE;
    dispShowAwakeSetup();
  }
}
void dispShowMenuSetup(){
  LCD.clear();
  LCD.cursor();
  LCD.setCursor(0, 0);
  LCD.write(T_ID);
  LCD.setCursor(8, 1);
  LCD.write(H_ID);
  LCD.setCursor(0, 1);
  LCD.write(CD_ID);
  LCD.setCursor(8, 0);
  LCD.write(L_ID);
  LCD.setCursor(13, 1);
  LCD.print("S");
  LCD.setCursor(15, 1);
  LCD.print("W");
}
void dispShowSleepSetup(){
  LCD.clear();
  LCD.noCursor();
  LCD.setCursor(0,0);
  LCD.print("Bedtime");
  LCD.setCursor(0,1);
  LCD.print("00:00");
}
void dispShowAwakeSetup(){
  LCD.clear();
  LCD.noCursor();
  LCD.setCursor(0,0);
  LCD.print("Awakening time");
  LCD.setCursor(0,1);
  LCD.print("00:00");
}
void butPressed(){
  flag = false;
}