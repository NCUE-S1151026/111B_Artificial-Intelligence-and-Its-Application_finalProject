/*
Author:           Josh Hsieh
well done Date:   2023-06-08
Description:      For NCUE 111B Technology Application and Innovation-00258
Advisor:          
*/

#include <WiFi.h>
#include "ThingSpeak.h"
#include <RTClib.h>
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include "DHT.h"
#include <Adafruit_ADXL345_U.h>
#include <LiquidCrystal_PCF8574.h>
#include <SoftwareSerial.h>
#include "soc/soc.h"             
#include "soc/rtc_cntl_reg.h"    
//用於電源不穩不重開機

#define spareTime 15
//間格測資時間

#define waterPin 34  
//analogRead
// #define errorPin 27 
//error LED
#define drainPin 33  
//電磁水閥relay
#define dht11Pin 18
//DHT11 G18
#define DHTTYPE DHT11

#define PMS_RXPin 25
#define PMS_TXPin 26
//PMS3003
#define LED1RPin 15
#define LED1GPin 16
#define LED1BPin 17
#define LED2RPin 19
#define LED2GPin 27
#define LED2BPin 14

/*-------------------------------------
RGB LED pin/ light staus 
mode  1 --  WIFI connecting    	      	1RG
mode  2 --  WIFI upload details	      	1B
mode  3 --  WIFI unload all success 	  1G
mode  4 --  WIFI upload details error	  1R
mode  5 --  getACCleration			        2RB
mode  6 --
mode  7 --  setup 			                2RG
mode  8 --  setup success			          2RGB
mode  9 --  PMS3003			                2B
mode 10 --  getWaterHight			          2G
mode 11 --  getBM				                2RB
mode 12 --  getDH				                2BG
mode 13 --  getWaterHight error	      	2R
mode 14 --
-------------------------------------
*/

//3003
SoftwareSerial PmsSerial(PMS_RXPin, PMS_TXPin); // RX, TX
// SoftwareSerial WifiSerial(4, 5); // RX, TX
int nFirstTime=1 ;
static unsigned int pm_cf_10;           //定義全域變數
static unsigned int pm_cf_25;
static unsigned int pm_cf_100;

//water
int baseWaterVal = 0 ; //基本底線水位
const int maxWaterVal = 2700 ; //最高上限水位
const int lowWaterVal = 500 ; //排水停止水位
int dyanWaterVal = 0 ; //動態偵測水位
int lastWaterVal = 0 ; //前一次水位值
int realWaterVal = 0 ; //本次實際水位值
float waterHight = 0 ; //輸出期間水位高度(毫米mm)

//3231
RTC_DS3231 rtc;
char t[32];

//BMP180
Adafruit_BMP085 bmp;
float bmpTemper = 0 ;
float bmpPreas = 0 ;

//ADXL345
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
float accX = 0 ; //x 加速度
float accY = 0 ; //y 加速度
float accZ = 0 ; //z 加速度

//DHT11
DHT dht(dht11Pin, DHTTYPE); // constructor to declare our sensor
float dhTemper = 0 ;
float dhHimi = 0 ;

//1602
LiquidCrystal_PCF8574 lcdB(0x23); 
LiquidCrystal_PCF8574 lcdG(0x27); 

//WIFI
const char* ssid = "JDHL02";   // your network SSID (name) 
const char* password = "josh1050239";   // your network password
// const char* ssid = "TANetRoaming";
//ThingSpeak
unsigned long myChannelNumber = 2167549;
const char * myWriteAPIKey = "ZRAC4P41C4FOSBGS";

//upoad 資料整理
float uploadLabel[8] = {bmpTemper, dhHimi, bmpPreas, waterHight, pm_cf_25, pm_cf_10, pm_cf_100};
String uploadLabelName[8] = {"bmpTemper", "dhHimi", "bmpPreas", "waterHight", "pm_cf_25", "pm_cf_10", "pm_cf_100"};
unsigned int testCount = 1 ;
int cccc = 0;

WiFiClient  client;


void setup() {
  // put your setup code here, to run once:
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  //關閉電源不穩就重開機的設定
  ledSatus(7,1);
  PmsSerial.begin(9600);
  PmsSerial.setTimeout(1500);
  Serial.begin(115200);
  setup1602(); 
  rtc.begin(); // Initialize DS3231
  Wire.begin(); // DS3231 use
  //rtc.adjust(DateTime(F(__DATE__),F(__TIME__))); //set Ds3231 time//set time use || remember to mask after set time!!!!!!!!!!!!

  WiFi.mode(WIFI_STA);   
  ThingSpeak.begin(client);  // Initialize ThingSpeak

  setupLED() ;
  dht.begin() ; // Initialize DHT11
  bmp.begin() ; // Initialize BMP180
  setupWaterHight();
  ledSatus(7,0);
  ledSatus(8,1);

  lcdG.setCursor(0, 0);
  lcdG.print(printDateTen());
  lcdG.setCursor(0, 1);
  lcdG.print(printTimeEig());
  lcdB.setCursor(0, 0);
  lcdB.print("setup success");
  lcdB.setCursor(0, 1);
  lcdB.print(WiFi.macAddress());
  delay(1500);
  ledSatus(8,0);
  lcdB.clear();
  lcdB.setCursor(0, 0);
  lcdB.print("WIFI:"+ String(ssid));
}

void setup1602(){
  lcdB.begin(16, 2); // initialize the lcd
  lcdG.begin(16, 2); // initialize the lcd
  lcdB.setBacklight(200);
  lcdB.home();
  lcdB.clear();
  lcdG.setBacklight(200);
  lcdG.home();
  lcdG.clear();
}

void setupLED(){
  pinMode(LED1RPin, OUTPUT);
  pinMode(LED1GPin, OUTPUT);
  pinMode(LED1BPin, OUTPUT);
  pinMode(LED2RPin, OUTPUT);
  pinMode(LED2GPin, OUTPUT);
  pinMode(LED2BPin, OUTPUT);
}

void setupWaterHight(){
  pinMode(waterPin, INPUT);
  pinMode(drainPin, OUTPUT);
  
  baseWaterVal = analogRead(waterPin); //設定基礎水位

  // while(baseWaterVal < lowWaterVal){ //液位面不足，加水
  //   //show error message on 1602//
  //   lcdB.clear();
  //   lcdB.setCursor(0, 0);
  //   lcdB.print("water drain error");
  //   baseWaterVal = analogRead(waterPin);
  //   ledSatus(13,0);
  //   delay(4000);
  //   ledSatus(13,0);
  //   delay(500);
  // }
  lastWaterVal = baseWaterVal;
}

void getWaterHight(){
  ledSatus(10,1);
  dyanWaterVal = analogRead(waterPin);
  realWaterVal = dyanWaterVal - lastWaterVal ; //本次實際水位值(小時累計4095)
  waterHight = realWaterVal*0.0005539727 ;
  //輸出期間水位高度(毫米mm) = realWaterVal/4095 *60 *(28*28)/(144*144)
  lastWaterVal = dyanWaterVal ; //更新本次

  if(dyanWaterVal > maxWaterVal) { //執行排水
    while(dyanWaterVal > lowWaterVal){
      digitalWrite(drainPin, HIGH);
      delay(200) ; //校正!!!! 續測時間                          
    }
    digitalWrite(drainPin, LOW); //關閉水閥
    delay(30000);
    baseWaterVal = analogRead(waterPin); //設定基礎水位
    lastWaterVal = baseWaterVal;   
  } 
  Serial.print(waterHight) ;
  Serial.println("waterHight") ;
  ledSatus(10,0);
}

void getBM(){
  ledSatus(11,1);
  bmpTemper = bmp.readTemperature() ;
  bmpPreas = bmp.readPressure() ;
  //bmp.readAltitude() ;
  //bmp.readSealevelPressure() ;
  Serial.print(bmpTemper) ;
  Serial.println("bmpTemper") ;
  Serial.print(bmpPreas) ;
  Serial.println("bmpPressure") ;
  ledSatus(11,0);
}

void getDH(){
  ledSatus(12,1);
  dhHimi = dht.readHumidity();
  //Read the moisture content in %.
  dhTemper = dht.readTemperature();
  //uploadLabel[?] = dhTemper ;
  //Read the temperature in degrees Celsius
  // = dht.readTemperature(true);
  // true returns the temperature in Fahrenheit
  Serial.print(dhHimi) ;
  Serial.println("dhHimi") ;
  Serial.print(dhTemper) ;
  Serial.println("dhTemper") ;
  ledSatus(12,0);
}

void getAccleration(){
  ledSatus(5,1);
  /* Get a new sensor event */ 
  sensors_event_t event; 
  accel.getEvent(&event);
 
  /* Display the results (acceleration is measured in m/s^2) */
  accX = event.acceleration.x ;
  accY = event.acceleration.y ;
  accZ = event.acceleration.z ;
  Serial.print(accX) ;
  Serial.println("accX") ;
  Serial.print(accY) ;
  Serial.println("accY") ;
  Serial.print(accZ) ;
  Serial.println("accZ") ;
  //Serial.println("m/s^2 ");
  ledSatus(5,0);
}

void getG5(unsigned char ucData)//獲取G5的值
{
  static unsigned int ucRxBuffer[250];
  static unsigned int ucRxCnt = 0;
  ucRxBuffer[ucRxCnt++] = ucData;
  if (ucRxBuffer[0] != 0x42 && ucRxBuffer[1] != 0x4D)//資料頭判斷
  {
    ucRxCnt = 0;
    return;
  }
  
  if (ucRxCnt > 16)//資料位元數判斷//G5T為16
  {
       pm_cf_10=(int)ucRxBuffer[4] * 256 + (int)ucRxBuffer[5];      //大氣環境下PM2.5濃度計算        
       pm_cf_25=(int)ucRxBuffer[6] * 256 + (int)ucRxBuffer[7];
       pm_cf_100=(int)ucRxBuffer[8] * 256 + (int)ucRxBuffer[9];
      //  pm_at_10=(int)ucRxBuffer[10] * 256 + (int)ucRxBuffer[11];               
      //  pm_at_25=(int)ucRxBuffer[12] * 256 + (int)ucRxBuffer[13];
      //  pm_at_100=(int)ucRxBuffer[14] * 256 + (int)ucRxBuffer[15];

    if (pm_cf_25 >  999 || pm_cf_10 > 999 || pm_cf_100 >999)//如果PM2.5數值>1000，返回重新計算
    {
      ucRxCnt = 0;
      return;
    }

    ucRxCnt = 0;
    return;
  }
}

void get3003(){
  ///////42223OP
  ledSatus(9,1);
  PmsSerial.listen();
  if (PmsSerial.isListening())
  {
    Serial.println("PmsSerial.isListening");
  }else
  {
    Serial.println("PmsSerial.is not Listening");
  }

  while (PmsSerial.available()) {
    getG5(PmsSerial.read());
  }

  // this part is for retrieving useful values
  if (pm_cf_10 ==0 && pm_cf_25 ==0 && pm_cf_100 ==0)
  {
    Serial.println("All are 0s");
    // new added; restart
    nFirstTime=1;
    delay(1000);   // avoid to read too many times
    // return; //2256爛啟用
  }
  ledSatus(9,0);
  ///////42223ED
}

String printTimeEig(){
  DateTime now = rtc.now();
  String TTprint = "";

  if(int(now.hour())<10){
    TTprint = "0"+ String(now.hour()) + ":";
  }
  else
  {
    TTprint = String(now.hour()) + ":";
  }

  if(int(now.minute())<10){
    TTprint = TTprint + "0"+ String(now.minute()) + ":";
  }
  else{
    TTprint = TTprint + String(now.minute()) + ":";
  }
  

  if(int(now.second())<10){
    TTprint = TTprint + "0"+ String(now.second());
  }
  else{
    TTprint = TTprint + String(now.second());
  }
  

  return(TTprint);
}

String printDateTen(){
  DateTime now = rtc.now();
  String TDprint = (String(now.year()) + "/");


  if(int(now.month())<10){
    TDprint = TDprint + "0"+ String(now.month()) + "/";
  }
  else{
    TDprint = TDprint + String(now.month()) + "/";
  }
  

  if(int(now.day())<10){
    TDprint = TDprint + "0"+ String(now.day());
  }
  else{
    TDprint = TDprint + String(now.day());
  }
  
  return(TDprint);
}

void ledSatus(int modeW, bool boo){  //which mode? / on or off
  if(boo == 1){ //ON
    switch(modeW){
      case 1:
        digitalWrite(LED1RPin, HIGH);
        digitalWrite(LED1GPin, HIGH);
        break;
      case 2:
        digitalWrite(LED1BPin, HIGH);
        break;
      case 3:
        digitalWrite(LED1GPin, HIGH);
        break;
      case 4:
        digitalWrite(LED1RPin, HIGH);
        break;
      case 5:
        digitalWrite(LED2RPin, HIGH);
        digitalWrite(LED2BPin, HIGH);
        break;
      case 7:
        digitalWrite(LED2RPin, HIGH);
        digitalWrite(LED2GPin, HIGH);
        break;
      case 8:
        digitalWrite(LED2RPin, HIGH);
        digitalWrite(LED2GPin, HIGH);
        digitalWrite(LED2BPin, HIGH);
        break;
      case 9:
        digitalWrite(LED2BPin, HIGH);
        break;
      case 10:
        digitalWrite(LED2GPin, HIGH);
        break;
      case 11:
        digitalWrite(LED2RPin, HIGH);
        digitalWrite(LED2BPin, HIGH);
        break;
      case 12:
        digitalWrite(LED2BPin, HIGH);
        digitalWrite(LED2GPin, HIGH);
        break;
      case 13:
        digitalWrite(LED2RPin, HIGH);
        break;
    }
  }
  else{ //Off
    switch(modeW){
      case 1:
        digitalWrite(LED1RPin, LOW);
        digitalWrite(LED1GPin, LOW);
        break;
      case 2:
        digitalWrite(LED1BPin, LOW);
        break;
      case 3:
        digitalWrite(LED1GPin, LOW);
        break;
      case 4:
        digitalWrite(LED1RPin, LOW);
        break;
      case 5:
        digitalWrite(LED2RPin, LOW);
        digitalWrite(LED2BPin, LOW);
        break;
      case 7:
        digitalWrite(LED2RPin, LOW);
        digitalWrite(LED2GPin, LOW);
        break;
      case 8:
        digitalWrite(LED2RPin, LOW);
        digitalWrite(LED2GPin, LOW);
        digitalWrite(LED2BPin, LOW);
        break;
      case 9:
        digitalWrite(LED2BPin, LOW);
        break;
      case 10:
        digitalWrite(LED2GPin, LOW);
        break;
      case 11:
        digitalWrite(LED2RPin, LOW);
        digitalWrite(LED2BPin, LOW);
        break;
      case 12:
        digitalWrite(LED2BPin, LOW);
        digitalWrite(LED2GPin, LOW);
        break;
      case 13:
        digitalWrite(LED2RPin, LOW);
        break;
    }
  }
}

void loop() {
  DateTime now = rtc.now();
  if(((now.minute()) % spareTime) == 0 || testCount == 1){  //整點或第一次測資
    // put your main code here, to run repeatedly:

    // Connect or reconnect to WiFi
    if(WiFi.status() != WL_CONNECTED){
      lcdB.setCursor(0, 1);
      lcdB.print("try connect WIFI");
      Serial.print("Attempting to connect");
      while(WiFi.status() != WL_CONNECTED){
        ledSatus(1,1); //WIFI connect on 
        WiFi.begin(ssid, password); 
        delay(5000);     
      } 
    Serial.println("\nConnected.");
    lcdB.setCursor(0, 1);
    lcdB.print("WIFI connect OK ");
    ledSatus(1,0); //WIFI connect off
    delay(1500);
    }

    lcdB.clear();
    lcdG.clear();
    lcdB.setCursor(0, 0);
    lcdB.print(printDateTen());
    lcdB.setCursor(11, 0);
    lcdB.print("C"+String(testCount)); //益位注意
    lcdB.setCursor(0, 1);
    lcdB.print(printTimeEig());
    lcdB.setCursor(9, 1);
    lcdB.print(String(myChannelNumber));

    get3003();
    getWaterHight() ;
    getBM() ;
    getDH() ;

    //upload to thingSpeak(loop for 8 fields)
    uploadLabel[0] = bmpTemper ;
    uploadLabel[1] = dhHimi ;
    uploadLabel[2] = bmpPreas ;
    uploadLabel[3] = waterHight ;
    uploadLabel[4] = pm_cf_25 ;
    uploadLabel[5] = pm_cf_10 ;
    uploadLabel[6] = pm_cf_100 ;

    for(int i=1; i<8; i++){
      Serial.println(i);
      Serial.println(uploadLabel[i-1]);
      ledSatus(2,1);
      lcdG.clear();
      lcdG.setCursor(0, 0);
      lcdG.print(uploadLabelName[i-1]);
      lcdG.setCursor(1, 1);
      lcdG.print(uploadLabel[i-1]);
      int Fs = ThingSpeak.writeField(myChannelNumber,i, uploadLabel[i-1], myWriteAPIKey);
      cccc = 0;
      while(Fs != 200){
        ledSatus(4,1);
        Serial.println("  error channel. HTTP error code " + String(Fs));
        lcdG.setCursor(11, 0);
        lcdG.print("error");
        lcdG.setCursor(11, 1);
        lcdG.print(String(Fs));
        ledSatus(2,0);
        delay(5000);
        lcdG.setCursor(11, 1);
        lcdG.print("cE>"+String(cccc));
        delay(10000);
        ledSatus(4,0);
        cccc=cccc+1;
        if(cccc == 20){
          break;
        }
      }
        Serial.println("  upload SUCCESS");
        lcdG.setCursor(11, 0);
        lcdG.print("   UP");
        lcdG.setCursor(11, 1);
        lcdG.print("   OK");
        ledSatus(2,0);
        delay(15000);
    }
    Serial.println("ALL SUCCESS"); 
    lcdG.clear();
    lcdG.setCursor(0, 0);
    lcdG.print("C"+String(testCount)); //益位注意
    lcdG.setCursor(3, 1);
    lcdG.print("ALL success");
    testCount = testCount + 1; 
    delay(10000);
  }
  //隨時循環顯示
  for(int i=1; i<8; i++){
      lcdG.clear();
      lcdG.setCursor(0, 0);
      lcdG.print(uploadLabelName[i-1]);
      lcdG.setCursor(3, 1);
      lcdG.print(uploadLabel[i-1]);
      delay(3000);
  }
  lcdG.clear();
  lcdG.setCursor(0,0);
  lcdG.print("recTime");
  lcdG.setCursor(8,1);
  lcdG.print(printTimeEig());
  delay(3000);
  
  lcdG.clear();
  lcdG.setCursor(0,0);
  lcdG.print("tset Freq");
  lcdG.setCursor(12,1);
  lcdG.print(String(spareTime) +"M");
  delay(3000);
}