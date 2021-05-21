//SNTP syncronised clock for ESP8266 - NodeMCU 1.0 (ESP-12E Module)
//Copyright: Owen Duffy    2021/05/16

#define nMMSS 14 //D5
#define nDST 12 //D6
#define nDIM 13 //D7
#define PIN_CLK 5 //D1
#define PIN_DATA 4 //D2
#define PIN_LATCH 0 //D3

//#define HAVE_TM1637
//#define HAVE_SR74HC595
#define HAVE_HT16K33

#include <LittleFS.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <TimeLib.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <WiFiUdp.h>
#include <Ticker.h>
#include <DNSServer.h>

#ifdef HAVE_TM1637 //https://github.com/AKJ7/TM1637
#include <TM1637.h>
TM1637 tm(PIN_CLK,PIN_DATA); //clk,data
#endif
#ifdef HAVE_SR74HC595 //https://github.com/ameer1234567890/ShiftDisplay2
#include <ShiftDisplay2.h>
ShiftDisplay2 sr(PIN_LATCH,PIN_CLK,PIN_DATA,COMMON_ANODE,4,STATIC_DRIVE); //latchPin,clockPin,dataPin
#endif
#ifdef HAVE_HT16K33 //https://github.com/RobTillaart/HT16K33
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
char hostname[11]="ssc01";
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
int brightness=100;
int timezoneoffset,daylightsavingoffset,twelvehour;
char strtimezoneoffset[11]="600",strdaylightsavingoffset[11]="60",strbrightness[11]="100";
bool shouldSaveConfig = false;
WiFiManager wm;
WiFiManagerParameter custom_field;
int loopctr=0;
bool dim;

//----------------------------------------------------------------------------------
//callback notifying us of the need to save config
void cbSaveConfig () {
  Serial.println(F("Should save config"));
  shouldSaveConfig=true;
}
//----------------------------------------------------------------------------------
  void getconfig(){
  //clean FS, for testing
  // LittleFS.format();

  //read configuration from FS json
  Serial.println(F("mounting FS..."));

  if (LittleFS.begin()){
    Serial.println(F("mounted file system"));
    if (LittleFS.exists(F("/config.json"))) {
      //file exists, reading and loading
      Serial.println(F("reading config file"));
      File configFile = LittleFS.open("/config.json","r");
      if (configFile) {
        Serial.println(F("opened config file"));
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
        brightness=json["brightness"];
        twelvehour=json["twelvehour"];
        Serial.print("timezoneoffset: ");
        Serial.println(timezoneoffset);
        Serial.print("daylightsavingoffset: ");
        Serial.println(daylightsavingoffset);
        Serial.print("brightness: ");
        Serial.println(brightness);
        Serial.print("twelvehour: ");
        Serial.println(twelvehour);
        }
      else{
        Serial.println(F("failed to load json config"));
        }
      }
    }
    else{
      Serial.println(F("failed to mount FS"));
      }
  }
//----------------------------------------------------------------------------------
void cbTick1(){
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
void cbSaveParams(){
  Serial.println(F("[CALLBACK] cbSaveParams fired"));
  Serial.println("PARAM twelvehour = " + getParam("twelvehour"));
//  twelvehour=getParam("twelvehour")=="1";
  twelvehour=(getParam(F("twelvehour"))).toInt();
//  Serial.println(twelvehour);
}
//----------------------------------------------------------------------------------
String getParam(String name){
  //read parameter from server, for customhmtl input
  String value="";
  if(wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  return value;
}
//----------------------------------------------------------------------------------
void setup(){
  Serial.begin(9600);
  while (!Serial){;} // wait for serial port to connect. Needed for Leonardo only
  Serial.println(F("serial started"));

  Serial.print(F("\nSketch size: "));
  Serial.print(ESP.getSketchSize());
  Serial.print(F("\nFree size: "));
  Serial.print(ESP.getFreeSketchSpace());
  Serial.print(F("\n\n"));

  wm.setSaveConfigCallback(cbSaveConfig);
  getconfig();

  // setup custom parameters
  snprintf(strtimezoneoffset,sizeof(strtimezoneoffset),"%d",timezoneoffset);
  snprintf(strdaylightsavingoffset,sizeof(strdaylightsavingoffset),"%d",daylightsavingoffset);
  WiFiManagerParameter custom_timezoneoffset("timezoneoffset","Time zone offset (min)",strtimezoneoffset,10);
  WiFiManagerParameter custom_daylightsavingoffset("daylightsavingoffset","Daylight saving offset (min)",strdaylightsavingoffset,10);
  WiFiManagerParameter custom_brightness("brightness","Brightness (0-100)",strbrightness,10);
  wm.addParameter(&custom_timezoneoffset);
  wm.addParameter(&custom_daylightsavingoffset);
  wm.addParameter(&custom_brightness);
  String custom_radio_str;
  custom_radio_str.reserve(500);
  custom_radio_str=F("<p>Select 12/24 hour display:</p>");
  custom_radio_str+=F("<input style='width: auto; margin: 0 10px 0 10px;' type='radio' name='twelvehour' value='0' ");
  if(!twelvehour)custom_radio_str+=F("checked ");
  custom_radio_str+=F(">24 hour<br>");
  custom_radio_str+=F("<input style='width: auto; margin: 0 10px 0 10px;' type='radio' name='twelvehour' value='1' ");
  if(twelvehour)custom_radio_str+=F("checked ");
  custom_radio_str+=F(">12 hour<br>");
//  Serial.println(custom_radio_str);
  new (&custom_field) WiFiManagerParameter(custom_radio_str.c_str()); // custom html input
  wm.addParameter(&custom_field);
  wm.setSaveParamsCallback(cbSaveParams);

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
  //twelvehour result is collected by cbSaveParams()

  if(shouldSaveConfig){
    //save the custom parameters to FS
    Serial.println(F("saving config"));
    DynamicJsonDocument doc(200);
    doc["timezoneoffset"]=timezoneoffset;
    doc["daylightsavingoffset"]=daylightsavingoffset;
    doc["brightness"]=brightness;
    doc["twelvehour"]=twelvehour;
    File configFile = LittleFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println(F("failed to open config file for writing"));
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
    setSyncInterval(8*3600);
    }
#ifdef HAVE_TM1637
    tm.init();
//    tm.setBrightness(brightness*7/100);
#endif
#ifdef HAVE_HT16K33
    seg.begin(PIN_DATA,PIN_CLK);//data,clk
    Wire.setClock(100000);
    seg.displayOn();
    seg.setDigits(4);
//    seg.brightness(brightness*15/100);
#endif

  ticker1.attach_ms(500,cbTick1);
}
//----------------------------------------------------------------------------------
void loop(){
  if (tick1Occured == true){
    tick1Occured = false;
    loopctr=loopctr+1;
    t=now()+timezoneoffset*60;
    if(!digitalRead(nDST))t+=daylightsavingoffset*60;
#ifdef HAVE_TM1637
    if(!(loopctr&0x7f))dim=digitalRead(nDIM);
    if(dim)tm.setBrightness(brightness*7/400);
    else tm.setBrightness(brightness*7/100);
    if(twelvehour){
      if(!digitalRead(nMMSS))sprintf(ts2,"%02d%02d",minute(t),second(t));
      else sprintf(ts2,"%2d%02d",hourFormat12(t),minute(t));
      }
    else{
      if(!digitalRead(nMMSS))sprintf(ts2,"%02d%02d",minute(t),second(t));
      else sprintf(ts2,"%2d%02d",hour(t),minute(t));
      }
    String ts6=ts2;
    tm.display(ts6);
    tm.switchColon();
    tm.refresh();
#endif
#ifdef HAVE_SR74HC595
    if(twelvehour){
      if(!digitalRead(nMMSS))sprintf(ts2,"%02d%02d",minute(t),second(t));
      else sprintf(ts2,"%2d%02d",hourFormat12(t),minute(t));
      }
    else{
      if(!digitalRead(nMMSS))sprintf(ts2,"%02d%02d",minute(t),second(t));
      else sprintf(ts2,"%2d%02d",hour(t),minute(t));
      }
    String ts6=ts2;
    sr.clear();
    sr.set(ts2);
    sr.update();
#endif
#ifdef HAVE_HT16K33
    if(!(loopctr&0x7f))dim=digitalRead(nDIM);
      if(dim)seg.brightness(brightness*15/400);
    else seg.brightness(brightness*15/100);
if(twelvehour){
      if(!digitalRead(nMMSS))seg.displayTime(minute(t),second(t),loopctr%2,true);
      else seg.displayTime(hourFormat12(t),minute(t),loopctr%2,false);
      }
    else{
      if(!digitalRead(nMMSS))seg.displayTime(minute(t),second(t),loopctr%2,true);
      else seg.displayTime(hour(t),minute(t),loopctr%2,false);
      }
#endif
  }
}
