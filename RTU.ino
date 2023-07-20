#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#ifndef STASSID
#define STASSID "HUAWEI-4049"
#define STAPSK  "personalwifisss1"
#endif

const char* mqtt_server = "20.163.192.238";
const int mqtt_port = 1883;

const char* ssid = STASSID;
const char* password = STAPSK;

WiFiClient espClient;
PubSubClient client(espClient);
WiFiServer telnetServer(23);
WiFiClient telnet;

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    telnet.print(".");
  }

  telnet.println("");
  telnet.println("WiFi connected");
  telnet.println("IP address: ");
  telnet.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  telnet.print("Message arrived [");
  telnet.print(topic);
  telnet.print("] ");
  for (unsigned int i = 0; i < length; i++) {
    telnet.print((char)payload[i]);
  }
  telnet.println();
}

void reconnect() {
  while (!client.connected()) {
    telnet.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
      telnet.println("connected");
      client.subscribe("flowRate");
    } else {
      telnet.print("failed, rc=");
      telnet.print(client.state());
      telnet.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  telnetServer.begin();
  telnetServer.setNoDelay(true);
  ArduinoOTA.begin();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  ArduinoOTA.handle();
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, password);
  }

  if (telnetServer.hasClient()){
    if (!telnet || !telnet.connected()){
      if(telnet) telnet.stop();
      telnet = telnetServer.available();
    }
  }
}
