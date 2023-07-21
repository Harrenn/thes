#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <BlynkSimpleEsp8266.h>

#ifndef STASSID
#define STASSID "HUAWEI-4049"
#define STAPSK  "personalwifisss1"
#endif

#define SENSOR  2  // Change here
#define LEAK_THRESHOLD 0.20 // 20% threshold for leak detection

volatile byte pulseCount;
byte pulse1Sec = 0;
float calibrationFactor = 4.5;
float flowMilliLitresPerMinute;  // Changed to float
float receivedFlowRate = 0.0;    // For storing received flow rate
unsigned long previousMillis = 0;
bool leakDetected = false;

unsigned long leakStartTime = 0; // For keeping track of leak start time
const unsigned long leakTimeThreshold = 60000;  // 10 seconds

char auth[] = "_rT-A_f1jKTxycyowJ-kkFkVaieceJN2"; // Add your Blynk token here

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

const char* mqtt_server = "20.163.192.238";
const int mqtt_port = 1883;

const char* ssid = STASSID;
const char* password = STAPSK;

WiFiClient espClient;
PubSubClient client(espClient);
WiFiServer telnetServer(23);
WiFiClient telnet;

BlynkTimer timer;

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

  // Convert payload to string
  String receivedPayload;
  for (unsigned int i = 0; i < length; i++) {
    receivedPayload += (char)payload[i];
  }

  // Convert string payload to float
  receivedFlowRate = receivedPayload.toFloat();
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

void checkForLeaks() {
  float flowRateDifference = abs(flowMilliLitresPerMinute - receivedFlowRate);
  if (flowRateDifference / flowMilliLitresPerMinute > LEAK_THRESHOLD) {
    if (!leakDetected && (millis() - leakStartTime > leakTimeThreshold)) {
      leakDetected = true;
      telnet.println("Leak detected");
      Blynk.virtualWrite(V31, "Leak detected");
    } else if (!leakDetected) {
      leakStartTime = millis();
    }
  } else {
    leakDetected = false;
    leakStartTime = 0;
    telnet.println("No leak detected");
    Blynk.virtualWrite(V31, "No leak detected");
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  pinMode(SENSOR, INPUT_PULLUP);
  pulseCount = 0;
  flowMilliLitresPerMinute = 0.0;
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);

  previousMillis = millis();

  telnetServer.begin();
  telnetServer.setNoDelay(true);
  ArduinoOTA.begin();

  Blynk.begin(auth, ssid, password);

  timer.setInterval(1000L, checkForLeaks);
}

void calculateFlow() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > 1000) {
    pulse1Sec = pulseCount;
    pulseCount = 0;

    float flowRateLperMin = ((1000.0 / (currentMillis - previousMillis)) * pulse1Sec) / calibrationFactor;
    flowMilliLitresPerMinute = flowRateLperMin * 1000; // L/min to mL/min

    previousMillis = currentMillis;

    telnet.print("Flow rate: ");
    telnet.print(flowMilliLitresPerMinute, 2);  // Prints the flowRate in mL/min to 2 decimal places
    telnet.println(" mL/min");

    Blynk.virtualWrite(V32, flowMilliLitresPerMinute); // Send flow rate data to Blynk
  }
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

  Blynk.run();
  timer.run(); // Initiates BlynkTimer
  calculateFlow();
}
