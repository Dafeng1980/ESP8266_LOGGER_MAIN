
/*ESP8266 model: ESP-01S Board SDA = 0; SCL = 2; ESP-12F Board SDA = 4; SCl = 0;            
 *  Author Dafeng 2023
*/
#include <Wire.h>
// #include <SD.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ArduinoLog.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include "max6675.h"
#include <EasyNTPClient.h>
#include <WiFiUdp.h>
#include <U8g2lib.h>

#define DEVICE_ID_Topic "npi/"
#define TWI_BUFFER_SIZE (256)
#define PS_I2C_ADDRESS 0x58
#define MSG_BUFFER_SIZE  (1024)
#define UI_BUFFER_SIZE (256)
#define LM73_ADDR (0x49)
#define LOG_LEVEL LOG_LEVEL_VERBOSE
#define SECONDS_FROM_1970_TO_2000  946684800 //< Unixtime for 2000-01-01 00:00:00, useful for initialization
//#define MQTT_MAX_PACKET_SIZE 1024  
WiFiClient eClient;
PubSubClient client(eClient);
WiFiUDP udp;
// EasyNTPClient ntpClient(udp, "pool.ntp.org", ((5*60*60)+(30*60))); // IST = GMT + 5:30
EasyNTPClient ntpClient(udp, "ntp.aliyun.com", ((8*60*60)+(0*60))); // CNT = GMT + 8:00
// EasyNTPClient ntpClient(udp, "ntp.aliyun.com", ((0*60*60)+(0*60))); // GMT + 0:00

const int thermoDO = 14;
const int thermo_aCS = 5;
const int thermoCLK = 16;
const int thermo_bCS = 15;
const int SDA_PIN = 4;         //ESP-01S Board SDA = 2; SCL = 0;   
const int SCL_PIN = 0;          //ESP8266 HEKR 1.1 Board  SDA = 14; SCL = 0; kLedPin = 4;
const uint8_t kLedPin = 12;      // ESP-12F Board SDA = 4; SCl = 0; kLedPin = 12;
const uint8_t kButtonPin = 13;     
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL_PIN, /* data=*/ SDA_PIN);   // pin remapping with ESP8266 HW I2C
MAX6675 thermocouple_a(thermoCLK, thermo_aCS, thermoDO);   //
MAX6675 thermocouple_b(thermoCLK, thermo_bCS, thermoDO);   //

static struct LoggerData
{
  float temp8D;
  float temp8E;
  float temp8F;
  float fanSpeed;
  float tempKa;
  float tempKb;
  float tempLm73;
  uint16_t statusWord;
  unsigned long unixtime;
  uint8_t s_tm; 
}ld;

static struct DateTime
{
  uint16_t year;
  uint8_t month;
  uint8_t week;
  uint8_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  unsigned long unixtime; 
}dt;

static struct eeprom
{
  uint8_t host;
  char ssid[16];
  char password[16];
  char mqtt_broker[31];
}eep;

uint8_t smbus_data[256];
static uint8_t eepbuffer[256];
static uint8_t ps_i2c_address;
static uint16_t lgInterval = 1000;  //PMbus refresh rate (miliseconds) 

static bool Protocol = true;   // If true, endTransmission() sends a stop message after transmission, releasing the I2C bus.
static bool wifistatus = true;
static bool mqttflag = false;
static bool dataflag = true;
static bool buttonflag = true;
static bool pmbusflag = true;
static bool pecflag = false;
static bool commandflag = false;
// const char* ssid = "FAIOT";           // Enter your WiFi name Local Network
const char* ssid = "K40IOT";           // Enter your WiFi name Local Network
const char* password = "20212021";    // Enter WiFi password
const char* mqtt_user = "dfiot";      //Raspberry MQTT Broker
const char* mqtt_password = "123abc";
// const char* mqtt_server = "192.168.200.2";   //Local broker service 
const char* mqtt_server = "xxx.xxx.xxx.xxx";   
const uint16_t mqtt_port =  1883;
const char* clientID = "zhsnpi1fdevices888";
const char hex_table[]{'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
const uint8_t daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30};

char mqtt_topic[50] = DEVICE_ID_Topic;
char msg[MSG_BUFFER_SIZE];
char ui_buffer[UI_BUFFER_SIZE];
char displaybuff[16];
unsigned long previousMillis = 0;
unsigned long rtcMillis = 0;
long count = 0;
uint16_t value = 0;

// PubSubClient client(mqtt_server, mqtt_port, eClient);
// DynamicJsonDocument doc(1024);
// JsonObject pmbus = doc.createNestedObject("pmbus");

void setup() { 
    pinMode(kButtonPin, INPUT_PULLUP);
    pinMode(kLedPin, OUTPUT);
    digitalWrite(kLedPin, LOW);
    Serial.begin(38400);
    EEPROM.begin(512);
    Log.begin(LOG_LEVEL, &Serial, false);  //
    Wire.begin(SDA_PIN, SCL_PIN);
    u8g2.begin();
    u8g2.clearBuffer();					// clear the internal memory
    u8g2.setFont(u8g2_font_ncenB08_tr);	// choose a suitable font
    u8g2.drawStr(0,10,"Logger Demo");	// write something to the internal memory
    u8g2.sendBuffer();					// transfer internal memory to the display 
    delay(10);
    if (digitalRead(kButtonPin) == 0){
          delay(10);
        if(digitalRead(kButtonPin) == 0) 
        {
          delay(600);
          set_wifi2eeprom();
        }       
       }   
    setWifiMqtt();
    delay(1800); 
    printhelp();
    ld.unixtime = ntpClient.getUnixTime();
    // unixtime2datatime(ld.unixtime);
    // u8g2.clearBuffer();
    // snprintf (displaybuff, 16, "%04d-%02d-%02d", dt.year, dt.month, dt.day);
    // u8g2.drawStr(0,10, displaybuff);
    // snprintf (displaybuff, 16, "%02d:%02d:%02d", dt.hour, dt.min, dt.sec);
    // u8g2.drawStr(0,30, displaybuff);
    // u8g2.sendBuffer();
    // delay(13600); 
 }

void loop() { 
   rtcUnixtime();    
   mqttLoop();
   commandcheck();
   checkSensors();
}


void oled_display(){
          u8g2.clearBuffer();					
          u8g2.setFont(u8g2_font_ncenB08_tr);
          snprintf (displaybuff, 16, "Ka: %2.0f Kb: %2.0f", ld.tempKa, ld.tempKb);	
          u8g2.drawStr(0,10, displaybuff);
          snprintf (displaybuff, 16, "Lm73: %2.1f", ld.tempLm73);
          u8g2.drawStr(0,20, displaybuff);
          snprintf (displaybuff, 16, "RC: %d", count);
          u8g2.drawStr(0,30, displaybuff);
          u8g2.sendBuffer();
      }

void unixtime2datatime(uint32_t t) {
  t -= SECONDS_FROM_1970_TO_2000; // bring to 2000 timestamp from 1970
  dt.sec = t % 60;
  t /= 60;
  dt.min = t % 60;
  t /= 60;
  dt.hour = t % 24;
  uint16_t days = t / 24;
  dt.week = (days + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
  uint8_t leap;
  for (dt.year = 0;; ++dt.year) {
    leap = dt.year % 4 == 0;
    if (days < 365U + leap){
      dt.year = dt.year + 2000;
      break;
    }
    days -= 365 + leap;
  }
  for (dt.month = 1; dt.month < 12; ++dt.month) {
    uint8_t daysPerMonth = daysInMonth[dt.month - 1];
    if (leap && dt.month == 2)
      ++daysPerMonth;
    if (days < daysPerMonth)
      break;
    days -= daysPerMonth;
  }
  dt.day = days + 1;
}

void displaydatatime(){
    unixtime2datatime(ld.unixtime);
    u8g2.clearBuffer();
    snprintf (displaybuff, 16, "%04d-%02d-%02d", dt.year, dt.month, dt.day);
    u8g2.drawStr(0,10, displaybuff);
    snprintf (displaybuff, 16, "%02d:%02d:%02d", dt.hour, dt.min, dt.sec);
    u8g2.drawStr(0,30, displaybuff);
    u8g2.sendBuffer();
}
