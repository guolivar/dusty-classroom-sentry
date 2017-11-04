/* Wireless ODIN-SD
WEMOS-D1-pro based ODIN-SD outdoor dust sensor
Software Serial communication with PMS3003
Clock checked via NTP
Data upload to Thingspeak server
 */

 #include <Arduino.h>
//Wireless and web-server libraries
#include "ESP8266WiFi.h"
#include <WiFiClient.h>

//Thingspeak library
#include "ThingSpeak.h"

// SD card libraries
#include "SPI.h"
#include "SD.h"

//SHT30 T&RH sensor
#include <WEMOS_SHT3X.h>

//OLED display
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#define OLED_RESET 0  // GPIO0



//Constants and definitions
#define chipSelect D8 // Where is the SD card chipSelect pin
#define receiveDatIndex 24 // Sensor data payload size
// PMS3003 variables
byte receiveDat[receiveDatIndex]; //receive data from the air detector module
byte readbuffer[128]; //full serial port read buffer for clearing
unsigned int checkSum,checkresult;
unsigned int FrameLength,Data4,Data5,Data6;
unsigned int PM1,PM25,PM10,PM1us,PM25us,PM10us,Data4,Data5,Data6;
int length,httpPort; //HTTP message variables
bool valid_data,iamok; // Instrument status
File myFile; //Output file
File configFile; // To read the configuration from sd card
String curr_time; //Date-Time objets
char eol='\n';
String sdCard,fname;
String ssid,passwd,srv_addr,private_key,public_key,port;
String serialn,t_res;
unsigned int interval;
char file_fname[50],c_ssid[50],c_passwd[50],c_serialn[50];
char c_srv_addr[50],c_private_key[50],c_public_key[50];
// To get the MAC address for the wireless link
uint8_t MAC_array[6];
char MAC_char[18];
// SHT30 T&RH sensor
SHT3X sht30(0x45);
//OLED display
Adafruit_SSD1306 display(OLED_RESET);
//ThingSpeak
unsigned long myChannelNumber = 31461;
const char * myWriteAPIKey = "LD79EOAAWRVYF04Y";



void setup() {
  iamok = true;
  Serial.begin(9600);
  Serial.setTimeout(20000);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);

}

void loop() {





  // Clear the buffer.
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.setTextColor(WHITE);

  if(sht30.get()==0){
    display.println("T: ");
    display.setTextSize(2);
    display.println(sht30.cTemp);

    display.setTextSize(1);
    display.println("H: ");
    display.setTextSize(2);
    display.println(sht30.humidity);
  }
  else
  {
    display.println("Error!");
  }
  display.display();
  delay(1000);






}

void readDust(){
	while (Serial.peek()!=66){
		receiveDat[0]=Serial.read();
		yield();
	}
	Serial.readBytes((char *)receiveDat,receiveDatIndex);
	checkSum = 0;
	for (int i = 0;i < receiveDatIndex;i++){
		checkSum = checkSum + receiveDat[i];
	}
	checkresult = receiveDat[receiveDatIndex-2]*256+receiveDat[receiveDatIndex-1]+receiveDat[receiveDatIndex-2]+receiveDat[receiveDatIndex-1];
	valid_data = (checkSum == checkresult);
}

String getTime() {
  WiFiClient client;
  while (!!!client.connect("google.com", 80)) {
    Serial.println("connection failed, retrying...");
  }

  client.print("HEAD / HTTP/1.1\r\n\r\n");

  while(!!!client.available()) {
     yield();
  }

  while(client.available()){
    if (client.read() == '\n') {
      if (client.read() == 'D') {
        if (client.read() == 'a') {
          if (client.read() == 't') {
            if (client.read() == 'e') {
              if (client.read() == ':') {
                client.read();
                String theDate = client.readStringUntil('\r');
                client.stop();
                return theDate;
              }
            }
          }
        }
      }
    }
  }
}

void initWifi() {
   Serial.print("Connecting to ");
   Serial.print(ssid);
   if (strcmp (WiFi.SSID(),ssid) != 0) {
      WiFi.begin(ssid, password);
   }

   while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
   }
  Serial.print("\nWiFi connected, IP address: ");
  Serial.println(WiFi.localIP());
}
