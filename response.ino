void sendServerResponse(const char* responseStr, int errorCode = 200)
{
  webServer.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  webServer.sendHeader("Pragma", "no-cache");
  webServer.sendHeader("Expires", "-1");
  webServer.send(errorCode, "text/html", ""); // Empty content inhibits Content-length header so we have to close the socket ourselves.
  webServer.sendContent(responseStr);
  webServer.client().stop();
}

void handleOn()
{
  waterHeaterCtrl.TurnOn();
  sendServerResponse("On");
  updateState();
}

void handleOff()
{
  waterHeaterCtrl.TurnOff();
  sendServerResponse("Off");
  updateState();
}

void handleState()
{  
  sendServerResponse(waterHeaterCtrl.GetState() ? "Heater is On" : "Heater is Off");
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  DEBUG();

  // Switch on the LED if an 1 was received as first character
  if (strncmp((const char*)payload,"ON",length)==0) {
    DEBUG("MQTT down received");
    waterHeaterCtrl.TurnOn();
    updateState();
  } 
  else if(strncmp((const char*)payload,"OFF",length)==0) {
    DEBUG("MQTT up received");
    waterHeaterCtrl.TurnOff();
    updateState();
  }
}

void onButtonClicked()
{
  // switch the state of the heater
  waterHeaterCtrl.TurnBool(!waterHeaterCtrl.GetState());
  updateState();
}

void updateState()
{
  mqttUpdateState = true;
  
  if(waterHeaterCtrl.GetState()){
    greenLed.TurnOn();
  }
  else{
    greenLed.TurnOff();  
  }
}

void mqttClientReconnect() {
    DEBUG("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(MQTT_CLIENT_NAME.c_str())) {
      DEBUG("connected");
      mqttClient.subscribe(MQTT_TOPIC_COM.c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      DEBUG(" try again later");
      redLed.Blink(50);
    }  
}

void handleResetSettings()
{
  iniFile.clearAll();
  delay(100);
  ESP.restart();
}

