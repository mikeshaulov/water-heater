#define CFG_INI_FILE "/cfg.ini"
#define DEBUG(x) Serial.println(x)
const char* CODE_VERSION = "1.0.0";

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <PubSubClient.h>

#include <switch.h>
#include <config.h>
#include <WiFiConfigurator.h>
#include <WiFiUpdater.h>

#include "FS.h" 

CReverseSwitch greenLed(12);
CReverseSwitch redLed(13);
CReverseSwitch blueLed(14);

// water heater pin controller
CLed waterHeaterCtrl(4);

// maximum time to heat the water
int maxHeatingTime;

ESP8266WebServer webServer(80);
IniFile iniFile(CFG_INI_FILE);
WiFiConfigurator wifiConfigurator("configure_water_heater",&iniFile);

String MQTT_CLIENT_NAME;
String MQTT_SERVER;
String MQTT_TOPIC_COM;
String MQTT_TOPIC_STATE;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
volatile bool mqttUpdateState = false;

// mount the file system
void mountFS()
{
  bool mountResult = SPIFFS.begin();
  if(mountResult)
    DEBUG("FS mount result: " + String(mountResult,DEC));
}

// captive portal for configuration
void startCaptive()
{
    blueLed.TurnOn();
    DEBUG("failed to load WiFi settings - setting wifiConfigurator AP");
    wifiConfigurator.startCaptive();
    blueLed.TurnOff();
}

void setup()
{                
  Serial.begin(115200);
  delay(500);
  attachInterrupt(5, onButtonClicked, RISING);

  mountFS();

 // if no configuration exists start the captive portal to get settings
  if(!iniFile.exists())
  {
    startCaptive();    
  }

  // try to connect to WiFi, if not - reset
  wifiConfigurator.connectToWiFi(&blueLed);

  // start MDNS
  wifiConfigurator.startMDNS();


  int maxHeatingTime = iniFile.getValue("MAX_TIME").toInt();
  if(!maxHeatingTime) maxHeatingTime = 1;
  
  // begin WebServer
  webServer.on("/on",handleOn);
  webServer.on("/off",handleOff);
  webServer.on("/state",handleState);
  webServer.on("/reset-settings", handleResetSettings);
  WiFiUpdater::setup(webServer);
  
  webServer.begin();
  
  // Add service to MDNS-SD
  MDNS.addService("http", "tcp", 80);

  // setup MQTT client settings

  // MQTT Client name must be unique
  MQTT_CLIENT_NAME = "ESPWaterHeater" + String(ESP.getChipId(),DEC);
  DEBUG("[MQTT Client Name] " + MQTT_CLIENT_NAME);

  MQTT_SERVER = iniFile.getValue("MQTT_SERVER");
  DEBUG(MQTT_SERVER);
  mqttClient.setServer(MQTT_SERVER.c_str(), 1883);    
  mqttClient.setCallback(mqtt_callback);
  
  MQTT_TOPIC_COM = iniFile.getValue("MQTT_TOPIC") + "com";
  DEBUG("[MQTT Command Topic] " + MQTT_TOPIC_COM);
  
  MQTT_TOPIC_STATE = iniFile.getValue("MQTT_TOPIC") + "state";  
  DEBUG("[MQTT State Topic] " + MQTT_TOPIC_STATE);
}

void loop()                     
{
  delay(50);
  
  if(wifiConfigurator.checkWiFiConnectivity(&redLed))
  {
    if(!mqttClient.connected())
      mqttClientReconnect();
    else {
      // check if state changed and needs to be republished
      if(mqttUpdateState)
      {
        mqttUpdateState = false;
        mqttClient.publish(MQTT_TOPIC_STATE.c_str(), waterHeaterCtrl.GetState() ? "ON" : "OFF");
      }
      mqttClient.loop();
    }
    
    onClearError();
    webServer.handleClient();    
    delay(100);
  }  
}

void onSetError(){redLed.TurnOn();}
void onClearError(){redLed.TurnOff();}


