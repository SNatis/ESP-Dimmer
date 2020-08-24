/*
  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#define PowerPin 14
#define zeroCross 12
// NOMAGE MQTT
#define ModuleName "Dimmer1"
#define TopicPower "Dimmer/1/Power"
#define TopicSet   "Dimmer/1/Set"
#define TopicState "Dimmer/1/State"
#define ESP8266Client ModuleName

long lastMsg = 0;
String request;
const char* ssid = "WifiName";   // replace with the SSID of your WiFi
const char* password = "Password";  // replace with your WiFi password
const char* host = "IP ou Host MQTT Broker";       // IP address or name of your MQTT broker

int tick = 720000;
int PowerOn = 0;
int Power = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void ICACHE_RAM_ATTR isr_ext() {
  timer1_write(tick);
}

void ICACHE_RAM_ATTR onTimerISR() {
  if (digitalRead(PowerPin) == LOW) {
    digitalWrite(PowerPin, HIGH);
    timer1_write(6960);
  } else {
    digitalWrite(PowerPin, LOW);
  }
}

//Fonction reconnexion MQTT
void reconnect() { 
  // Loop until we're reconnected
  char ch_status[1];
  //Serial.println("Reconnection");
  while (!client.connected()) {
    //Serial.println("Attempt to connect");
    if (client.connect(ESP8266Client)) {
      //Serial.println("Once connected, publish an announcement...");
      sprintf(ch_status, "%d", Power);  
      client.publish(TopicPower, ch_status);
      sprintf(ch_status, "%d", PowerOn);
      client.publish(TopicState, ch_status);
      // ... and resubscribe
      client.subscribe(TopicSet);
    } else {
      //Serial.println("Wait 5 seconds before retrying");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  int i = 0;
  char message_buff[100];
  // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';
  
  String msgString = String(message_buff);
  uint8_t niv = msgString.toInt();
  if (niv >= 0 && niv <= 100 &&  msgString != "on" && msgString != "off"){
    Power = niv;
    if (niv == 0) niv = 1;
    if (niv == 100) niv == 99;
    tick = (712001 * (1 - (float(niv) / 100))) + 7999;
  } else if ( msgString == "on" ) {
    PowerOn = 1;
    attachInterrupt(digitalPinToInterrupt(zeroCross), isr_ext, FALLING); 
  } else if ( msgString == "off" ){
    PowerOn = 0;
    detachInterrupt(digitalPinToInterrupt(zeroCross));
  }
  mqtt();
}

// FONCTION MQTT ESP VERS JEEDOM
void mqtt(){
  char ch_status[1];  
  
  if (!client.connected()) {
    reconnect();
  }
  else {
    sprintf(ch_status, "%d", Power);  
    client.publish(TopicPower, ch_status);
    sprintf(ch_status, "%d", PowerOn);
    client.publish(TopicState, ch_status);
 }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  //MQTT//
  client.setServer(host, 1883);
  client.setCallback(callback);
  
  pinMode(PowerPin, OUTPUT);
  pinMode(zeroCross, INPUT);
  timer1_attachInterrupt(onTimerISR);
  timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE);
}

void loop() {
  long now = millis();
  if (now - lastMsg > 100) {
    lastMsg = now;
      if (!client.connected()) {
      reconnect();
    }
  }
  client.loop();
}
