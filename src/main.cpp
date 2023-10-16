#include <Arduino.h>
#include "config.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

enum State {NORMAL, GREENHOUSE1, GREENHOUSE2};


char ssid[] = WIFIHOTSPOT;
char pw[] = WIFIKEY;
State state = NORMAL;
int noMessagesIn = 0;
int waterTics = 0;
int loops=0;
boolean overflow = false;

WiFiClient espClient;
PubSubClient client(espClient);

void setState(State newState) {
  state = newState;
  switch (state) {
    case State::GREENHOUSE1:
      digitalWrite(BYPASS, HIGH);
      digitalWrite(GREENHOUSE1, HIGH);
      digitalWrite(GREENHOUSE2, LOW);
      break;
    case State::GREENHOUSE2:
      digitalWrite(BYPASS, HIGH);
      digitalWrite(GREENHOUSE1, LOW);
      digitalWrite(GREENHOUSE2, HIGH);
      break;
    default:
      digitalWrite(BYPASS, LOW);
      digitalWrite(GREENHOUSE1, LOW);
      digitalWrite(GREENHOUSE2, LOW);
      break;
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);

   String topicStr = String(topic);
   String payloadStr = "";
   for (int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
   }
    Serial.print(payloadStr);
    Serial.println("] ");
    if(topicStr == COMMAND_TOPIC){
      if(payloadStr == "greenhouse1"){
        setState(State::GREENHOUSE1);  
      }else if(payloadStr == "greenhouse2"){
        setState(State::GREENHOUSE2);
      }else if(payloadStr == "normal"){
        setState(State::NORMAL);
      }}
   noMessagesIn=0;
}

void IRAM_ATTR reactToWaterTick(){
  waterTics++;
}

String stateToString(State state) {
  switch (state) {
    case State::NORMAL:
      return "normal";
    case State::GREENHOUSE1:
      return "greenhouse1";
    case State::GREENHOUSE2:
      return "greenhouse2";
    default:
      return "unknown";
  }
}

void sendStatus() {
  StaticJsonDocument<200> doc;
  doc["state"] = stateToString(state);
  doc["waterTics"] = waterTics;
  doc["overflow"] = overflow;
  String jsonStr;
  serializeJson(doc, jsonStr);
  client.publish(STATUS_TOPIC, jsonStr.c_str());
}

void setupWifi(){
    WiFi.begin(WIFIHOTSPOT, WIFIKEY);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    client.setServer(MQTT_SERVER, 1883);
    client.setCallback(callback);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(WATCHDOG_TOPIC);
      client.subscribe(COMMAND_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
  void IRAM_ATTR reactToOverflowChange() {
    overflow = !digitalRead(OVERFLOW);
    sendStatus();
  }


  void setup() {
    Serial.begin(9600);
    Serial.println("Setup-Start");
    pinMode(WATER_PIN, INPUT);
    pinMode(BYPASS, OUTPUT);
    pinMode(GREENHOUSE1PIN, OUTPUT);
    pinMode(GREENHOUSE2PIN, OUTPUT);
    pinMode(OVERFLOW, INPUT_PULLUP);
    Serial.println("Setup-Wifi");
    setupWifi();
    Serial.println("Setup-Interrupts");
    attachInterrupt(digitalPinToInterrupt(WATER_PIN), reactToWaterTick, FALLING);
    attachInterrupt(digitalPinToInterrupt(OVERFLOW), reactToOverflowChange, CHANGE);
  }

  
void loop() {
  
   if (!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(300);
  client.loop();
  loops++;
  if(loops%10==0){
    sendStatus();
  }
  Serial.println(".");
  delay(300);
  client.loop();
}
