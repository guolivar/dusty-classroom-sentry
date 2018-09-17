/* Wireless ODIN-SD
WEMOS-D1-pro based ODIN-SD outdoor dust sensor
Software Serial communication with PMS3003
Clock checked via NTP
Data upload to Thingspeak server

Libraries:
Adafruit_GFX
Adafruit_SSD1306
Phant
WEMOS_SHT3X

 */

#include <Arduino.h>
//Wireless and web-server libraries
#include "ESP8266WiFi.h"
#include <WiFiClient.h>

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

//Phant library
#include <Phant.h>

// Software Serial
#include "SoftwareSerial.h"
#define PMSTX D4
#define PMSRX D3

//Constants and definitions
#define chipSelect D8 // Where is the SD card chipSelect pin
#define receiveDatIndex 24 // Sensor data payload size
// PMS3003 variables
byte receiveDat[receiveDatIndex]; //receive data from the air detector module
byte readbuffer[128]; //full serial port read buffer for clearing
int nrecs; //to keep track of how many loops
unsigned int checkSum,checkresult;
unsigned int FrameLength,Data4,Data5,Data6;
unsigned int PM1,PM25,PM10,PM1us,PM25us,PM10us;
SoftwareSerial pms_port(PMSRX, PMSTX, false, 128);

bool valid_data,iamok; // Instrument status
File myFile; //Output file
File configFile; // To read the configuration from sd card
String curr_time; //Date-Time container
char eol='\n';
String sdCard,fname;
String ssid,passwd,srv_addr,private_key,public_key,port;
String serialn,t_res;
unsigned int interval,tic,toc;
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
unsigned long myChannelNumber;
char myWriteAPIKey[16];

String getTime(char * timesrv) {
  WiFiClient client;
  if (!!!client.connect(timesrv, 80)) {
    yield();
    Serial.println(F("connection failed, no time"));
    String theDate = F("no valid time");
    return theDate;
  } else {
    client.print(F("HEAD / HTTP/1.1\r\n\r\n"));

    while(!!!client.available()) {
      yield();
    }

    while(client.available()){
      yield();
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
}

void initWifi() {
   Serial.print(F("Connecting to "));
   Serial.print(ssid);
   WiFi.begin(c_ssid, c_passwd);
   // Try a few times and then timeout
   int tout = 0;
   while ((WiFi.status() != WL_CONNECTED)&&(tout<50)) {
     yield();
     delay(500);
     Serial.print(".");
     tout = tout +1;
   }
   if (WiFi.status() != WL_CONNECTED){
     Serial.println(F("WiFi not connected ... timed out"));
   }
  Serial.print(F("\nWiFi connected, IP address: "));
  Serial.println(WiFi.localIP());
}

void readDust(){
	while (pms_port.peek()!=66){
		receiveDat[0]=pms_port.read();
		yield();
    delay(5);
	}
	pms_port.readBytes((char *)receiveDat,receiveDatIndex);
	checkSum = 0;
	for (int i = 0;i < receiveDatIndex;i++){
		checkSum = checkSum + receiveDat[i];
	}
	checkresult = receiveDat[receiveDatIndex-2]*256+receiveDat[receiveDatIndex-1]+receiveDat[receiveDatIndex-2]+receiveDat[receiveDatIndex-1];
	valid_data = (checkSum == checkresult);
}
void setup() {
  iamok = true;
  Serial.begin(115200);
  pms_port.begin(9600);
  pms_port.setTimeout(20000);
  Serial.println(F("I'm starting"));
  //Read configuration file

  //Initialize the SD card and read configuration
	Serial.println(F("Initializing SD Card ..."));
	if (!SD.begin(chipSelect)){
		Serial.println(F("initialization failed!"));
		iamok = false;
		//return;
	}
	Serial.println(F("Initialization done."));
	//Read configuration options
	//wifi SSID
	//wifi PWD
	//serialnumber
	//time resolution (seconds)
	// Phant address
  // Phant Public Key
	// Phant Private Key
	configFile = SD.open("config.txt",FILE_READ);
	//Read SSID
	ssid = "";
	while (configFile.peek()!='\n'){
		ssid = ssid + String((char)configFile.read());
	}
  Serial.println(ssid);
	ssid.toCharArray(c_ssid,ssid.length()+1);
	eol=configFile.read();
	//Read password
	passwd = "";
	while (configFile.peek()!='\n'){
		passwd = passwd + String((char)configFile.read());
	}
  Serial.println(passwd);
	passwd.toCharArray(c_passwd,passwd.length()+1);
	eol=configFile.read();
	//Read Serial Number
	serialn = "";
	while (configFile.peek()!='\n'){
		serialn = serialn + String((char)configFile.read());
	}
  Serial.println(serialn);
	serialn.toCharArray(c_serialn,serialn.length()+1);
	eol=configFile.read();
	//Read time resolution
	t_res = "";
	while (configFile.peek()!='\n'){
		t_res = t_res + String((char)configFile.read());
	}
  Serial.println(t_res);
	interval = t_res.toInt();
	eol=configFile.read();
	//Read Phant server address
	srv_addr = "";
	while (configFile.peek()!='\n'){
		srv_addr = srv_addr + String((char)configFile.read());
	}
  Serial.println(srv_addr);
	srv_addr.toCharArray(c_srv_addr, srv_addr.length()+1);

	eol=configFile.read();
  //Read public_key
	public_key = "";
	while (configFile.peek()!='\n'){
		public_key = public_key + String((char)configFile.read());
	}
	public_key.toCharArray(c_public_key,public_key.length()+1);
	eol=configFile.read();
	//Read time private_key
	private_key = "";
	while (configFile.peek()!='\n'){
		private_key = private_key + String((char)configFile.read());
	}
	private_key.toCharArray(c_private_key,private_key.length()+1);

	configFile.close();

  //Create folder for this serial number
  bool mkd = SD.mkdir(c_serialn);
  Serial.println(mkd);
  sdCard = serialn;

	// Echo the config file
	Serial.println(c_ssid);
	Serial.println(c_passwd);
	Serial.println(serialn);
	Serial.println(interval);
	Serial.println(c_srv_addr);
	Serial.println(c_public_key);
	Serial.println(c_private_key);
  curr_time = "2000-01-01 00:00:00";

  //Start Wifi and ThingSpeak
  initWifi();
  WiFiClient client;
  curr_time = getTime(c_srv_addr);
  Phant phant(srv_addr, public_key, private_key);
  //Start a new run
  nrecs = 0;
}

void loop() {
  tic = millis();
  //Read data
	//Clear the serial receive buffer
	Serial.readBytes((char *)readbuffer,128);
  Serial.println(F("I'm about to read the dust sensor"));
  nrecs = nrecs+1;
  int i1;
  for (i1=0;i1<receiveDatIndex;i1++){
    receiveDat[i1]=0;
  }
  valid_data = (1 == 0);
	while ((!valid_data)){
		readDust();
    yield();
		delay(5);
	}
  sht30.get();
  Serial.println(F("I got data"));
	// Get time
  curr_time = getTime(c_srv_addr);
  // Parse the PMS3003 message
	FrameLength = (receiveDat[2]*256)+receiveDat[3];
	PM1 = (receiveDat[4]*256)+receiveDat[5];
	PM25 = (receiveDat[6]*256)+receiveDat[7];
	PM10 = (receiveDat[8]*256)+receiveDat[9];
	Data4 = (receiveDat[10]*256)+receiveDat[11];
	Data5 = (receiveDat[12]*256)+receiveDat[13];
	Data6 = (receiveDat[14]*256)+receiveDat[15];
	PM1us = (receiveDat[16]*256)+receiveDat[17];
	PM25us = (receiveDat[18]*256)+receiveDat[19];
	PM10us = (receiveDat[20]*256)+receiveDat[21];
	Serial.print(curr_time);
	Serial.print(F(" PM2.5 = "));
	Serial.println(PM25);
	fname =String(serialn + "/" + sdCard + ".txt");
	fname.toCharArray(file_fname,fname.length()+1);
	float temperature = sht30.cTemp;
	float rh = sht30.humidity;
  Serial.print(F("Temperature = "));
  Serial.println(temperature);
  //Write to SD card
  myFile = SD.open(file_fname, FILE_WRITE);
  myFile.print(curr_time);
  myFile.print("\t");
  myFile.print(PM1);
  myFile.print("\t");
  myFile.print(PM25);
  myFile.print("\t");
  myFile.print(PM10);
  myFile.print("\t");
  myFile.print(temperature);
  myFile.print("\t");
  myFile.print(rh);
  myFile.print("\t");
  myFile.println(millis());
  myFile.close();

  //Create Phant object
  Phant phant(srv_addr, public_key, private_key);
  // Add measurements to phant object
  Serial.println(F("Now to update fields"));

  phant.add("pm1", PM1);
  phant.add("pm25", PM25);
  phant.add("pm10", PM10);
  phant.add("temperature",temperature);
  phant.add("rh",rh);
  // Write the fields that you've set all at once.
  Serial.println(F("Now to upload"));
  WiFiClient client;
  if (client.connect(c_srv_addr,8080)){
    client.print(phant.post());
    client.stop();
  }
  else {
    Serial.println(F("Can't connect to Phant server!"));
  }

  toc = millis();
  unsigned long nap_time = (interval*1000) - (toc - tic);
  Serial.print(F("Go to sleep for "));
  Serial.println(nap_time);
  if (nrecs > 60) {
    setup();
  } else {
    delay(nap_time);
  }
}
