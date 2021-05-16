//HT16K33 1.2" display
//TM1637 https://github.com/AKJ7/TM1637
//74HC595
//#define DISPLAY TM1637
#define DISPLAY SR74HC595
//#define DISPLAY HT16K33
//SNTP syncronised clock for ESP8266 - NodeMCU 1.0 (ESP-12E Module)
//Copyright: Owen Duffy    2021/05/16

#include <LittleFS.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <TimeLib.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <DNSServer.h>

#if DISPLAY==TM1637
#include <TM1637.h>
TM1637 tm(0,2); //clk,data D3,D4
#endif
#if DISPLAY==SR74HC595
#include <ShiftDisplay2.h>
//ShiftDisplay2 sr(4,0,2,COMMON_ANODE,4,STATIC_DRIVE); //latchPin,clockPin,dataPin D2,D3,D4
ShiftDisplay2 sr(4,5,2,COMMON_ANODE,4,STATIC_DRIVE); //latchPin,clockPin,dataPin D2,D1,D4
#endif
#if DISPLAY==HT16K33
#include "HT16K33.h"
HT16K33 seg(0x70);
#endif

#if defined(ARDUINO_ARCH_ESP8266)
#include <ESP8266WiFi.h>
//#include <ESP8266WebServer.h>
#elif defined(ARDUINO_ARCH_ESP32)
#include <WiFi.h>
//#include <WebServer.h>
#endif
//#include <OneWire.h>
//#include <PageBuilder.h>
#if defined(ARDUINO_ARCH_ESP8266)
//ESP8266WebServer  server;
#elif defined(ARDUINO_ARCH_ESP32)
//WebServer  server;
#endif
#include <WiFiManager.h>

const char ver[]="0.01";
char hostname[11]="sclk01";
int t=0;
char name[21];
int i,j,ticks,interval;
bool tick1Occured,timeset;
const int timeZone=0;
static const char ntpServerName[]="pool.ntp.org";
WiFiUDP udp;
String header; //HTTP request
unsigned int localPort=8888; //local port to listen for UDP packets
Ticker ticker1;
String currentUri((char *)0);
char ts[21],ts2[10],configfilename[32];
byte present = 0;
byte data[12];
int brightness=0;
int timezoneoffset,daylightsavingoffset;
char strtimezoneoffset[11]="600",strdaylightsavingoffset[11]="60";
bool shouldSaveConfig = false;

//----------------------------------------------------------------------------------
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig=true;
}
//----------------------------------------------------------------------------------
  void getconfig(){
  //clean FS, for testing
  // LittleFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (LittleFS.begin()){
    Serial.println("mounted file system");
    if (LittleFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = LittleFS.open("/config.json","r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument doc(200); //https://arduinojson.org/v6/assistant/
        DeserializationError error = deserializeJson(doc, buf.get());
        if (error) {
          Serial.println(F("Failed to load JSON config"));
          while(1);
          }
        JsonObject json = doc.as<JsonObject>();
        Serial.println(F("\nParsed json"));

        timezoneoffset=json["timezoneoffset"];
        daylightsavingoffset=json["daylightsavingoffset"];
        Serial.print("timezoneoffset: ");
        Serial.println(timezoneoffset);
        Serial.print("daylightsavingoffset: ");
        Serial.println(daylightsavingoffset);
        }
      else{
        Serial.println("failed to load json config");
        }
      }
    }
    else{
      Serial.println("failed to mount FS");
      }
  }
//----------------------------------------------------------------------------------
void cbtick1(){
    tick1Occured=true;
}
//----------------------------------------------------------------------------------
const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while(udp.parsePacket() > 0); //discard any previously received packets
  Serial.println(F("Transmit NTP Request"));
  // get a random server from the pool
  WiFi.hostByName(ntpServerName,ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(F(": "));
  Serial.println(ntpServerIP);
  sendNTPpacket(ntpServerIP);
  uint32_t beginWait=millis();
  while (millis()-beginWait<1500) {
    int size=udp.parsePacket();
    if(size>=NTP_PACKET_SIZE){
      Serial.println(F("Received NTP response"));
      udp.read(packetBuffer,NTP_PACKET_SIZE); //read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900=(unsigned long)packetBuffer[40]<<24;
      secsSince1900|=(unsigned long)packetBuffer[41]<<16;
      secsSince1900|=(unsigned long)packetBuffer[42]<<8;
      secsSince1900|=(unsigned long)packetBuffer[43];
      return secsSince1900-2208988800UL+timeZone*SECS_PER_HOUR;
    }
  }
  Serial.println(F("No NTP response"));
  return 0; //return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer,0,NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0]=0b11100011; //LI, Version, Mode
  packetBuffer[1]=0; //Stratum, or type of clock
  packetBuffer[2]=6; //Polling Interval
  packetBuffer[3]=0xEC; //Peer Clock Precision
  //8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]=49;
  packetBuffer[13]=0x4E;
  packetBuffer[14]=49;
  packetBuffer[15]=52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(address,123); //NTP requests are to port 123
  udp.write(packetBuffer,NTP_PACKET_SIZE);
  udp.endPacket();
}
//----------------------------------------------------------------------------------
void setup(){
  WiFiManager wm;

  Serial.begin(9600);
  while (!Serial){;} // wait for serial port to connect. Needed for Leonardo only
  Serial.println("serial started");

  Serial.print(F("\nSketch size: "));
  Serial.print(ESP.getSketchSize());
  Serial.print(F("\nFree size: "));
  Serial.print(ESP.getFreeSketchSpace());
  Serial.print(F("\n\n"));

  wm.setSaveConfigCallback(saveConfigCallback);
  getconfig();

  // setup custom parameters
  snprintf(strtimezoneoffset,sizeof(strtimezoneoffset),"%d",timezoneoffset);
  snprintf(strdaylightsavingoffset,sizeof(strdaylightsavingoffset),"%d",daylightsavingoffset);
  WiFiManagerParameter custom_timezoneoffset("timezoneoffset","timezoneoffset",strtimezoneoffset,10);
  WiFiManagerParameter custom_daylightsavingoffset("daylightsavingoffset","daylightsavingoffset",strdaylightsavingoffset,10);
  wm.addParameter(&custom_timezoneoffset);
  wm.addParameter(&custom_daylightsavingoffset);
  //reset settings - wipe credentials for testing
  //wm.resetSettings();
 
  WiFi.hostname(hostname);
  wm.setDebugOutput(true);
  wm.setHostname(hostname);
  wm.setConfigPortalTimeout(120);
  Serial.println(F("Connecting..."));
  Serial.print(WiFi.hostname());
  Serial.print(F(" connecting to "));
  Serial.println(WiFi.SSID());
  wm.autoConnect("scccfg");
  if(WiFi.status()==WL_CONNECTED){
    Serial.println(WiFi.localIP().toString().c_str());

  timezoneoffset=atoi(custom_timezoneoffset.getValue());
  daylightsavingoffset=atoi(custom_daylightsavingoffset.getValue());
  if(shouldSaveConfig){
    //save the custom parameters to FS
    Serial.println("saving config");
    DynamicJsonDocument doc(200);
    doc["timezoneoffset"] = timezoneoffset;
    doc["daylightsavingoffset"] = daylightsavingoffset;
    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }
    char output[200];
    serializeJsonPretty(doc,output,200);
    Serial.print(output);
    configFile.print(output);
    configFile.close();
    //end save
    shouldSaveConfig = false;
    }
  
    // Print local IP address
    Serial.println(F(""));
    Serial.println(F("WiFi connected."));
    Serial.println(F("IP address: "));
    Serial.println(WiFi.localIP());
    Serial.println(F("Hostname: "));
    Serial.println(WiFi.hostname());

    Serial.println(F("Starting UDP"));
    udp.begin(localPort);
    Serial.print(F("Local port: "));
    Serial.println(udp.localPort());
    Serial.println(F("waiting for sync"));
    setSyncProvider(getNtpTime);
    timeset=timeStatus()==timeSet;
    setSyncInterval(36000);
    }
#if DISPLAY==TM1637
    tm.init();
    tm.setBrightness(7);
#endif
#if DISPLAY==HT16K33
    seg.begin(2,0);//data,clk D4,D3
    Wire.setClock(100000);
    seg.displayOn();
    seg.setDigits(4);
#endif
  WiFi.mode(WIFI_OFF);

//  ticker1.attach_ms(1000,cbtick1);
  ticker1.attach_ms(500,cbtick1);
}
//----------------------------------------------------------------------------------
void loop(){
  if (tick1Occured == true){
    tick1Occured = false;
    t=now()+timezoneoffset*60;
    if(!digitalRead(0))t+=daylightsavingoffset*60; //D1
    sprintf(ts2,"%02d%02d",hour(t),minute(t));
    String ts6=ts2;
    Serial.println(ts6);
#if DISPLAY==TM1637
    tm.display(ts6);
    tm.switchColon();
    tm.refresh();
//    brightness+=1;
//    brightness=brightness%16;
//    tm.setBrightness(brightness/2);
#endif
#if DISPLAY==SR74HC595
    sr.clear();
    sr.set(ts2);
    sr.update();
#endif
#if DISPLAY==HT16K33
    seg.displayTime(hour(t),minute(t));
    seg.displayColon(1);
#endif
  }
}
