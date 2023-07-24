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

#define SENSOR  2
#define LEAK_THRESHOLD 0.05

volatile byte pulseCount;
byte pulse1Sec = 0;
float calibrationFactor = 6.45;
float flowMilliLitresPerMinute;  
float receivedFlowRate = 0.0;
unsigned long previousMillis = 0;
bool leakDetected = false;

unsigned long leakStartTime = 0;
const unsigned long leakTimeThreshold = 5000;

char auth[] = "_rT-A_f1jKTxycyowJ-kkFkVaieceJN2";

void IRAM_ATTR pulseCounter() {
  pulseCount++;
}

const char* mqtt_server = "20.163.192.238";
const int mqtt_port = 1883;

const char* ssid = STASSID;
const char* password = STAPSK;

WiFiClient espClient;
PubSubClient client(espClient);

BlynkTimer timer;

#define MOVING_AVERAGE_PERIOD 50
float flowReadings[MOVING_AVERAGE_PERIOD];
int flowReadingsIndex = 0;
float smoothedFlowRate = 0.0;
float flowRateDifference = 0.0;

bool dataFromSensor = false;
unsigned long lastSensorCheck = 0;
const unsigned long sensorCheckInterval = 5000; 

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String receivedPayload;
  for (unsigned int i = 0; i < length; i++) {
    receivedPayload += (char)payload[i];
  }

  receivedFlowRate = receivedPayload.toFloat();
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP8266Client")) {
      client.subscribe("flowRate");
    } else {
      delay(5000);
    }
  }
}

void checkForLeaks() {
  flowRateDifference = abs(smoothedFlowRate - receivedFlowRate);
  
  if ((smoothedFlowRate == 0.0 && receivedFlowRate != 0.0) || 
      (flowRateDifference / smoothedFlowRate > LEAK_THRESHOLD)) {

    if (!leakDetected) {
      if (leakStartTime == 0) {
        leakStartTime = millis();
      } else if (millis() - leakStartTime > leakTimeThreshold) {
        leakDetected = true;
        Blynk.virtualWrite(V31, "Leak detected");
        Blynk.virtualWrite(V34, flowRateDifference);
        checkLeakageLevel();
      }
    } else {
      Blynk.virtualWrite(V34, flowRateDifference);
      Blynk.virtualWrite(V31, "Leak detected");
      checkLeakageLevel();
    }
  } else {
    if (leakDetected) {
      leakDetected = false;
      leakStartTime = 0;
      Blynk.virtualWrite(V31, "No leak detected");
    }
  }
}

void calculateFlow() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis > 1000) {
    pulse1Sec = pulseCount;
    pulseCount = 0;

    float flowRateLperMin = ((1000.0 / (currentMillis - previousMillis)) * pulse1Sec) / calibrationFactor;
    flowMilliLitresPerMinute = (flowRateLperMin * 1000) / 60;

    previousMillis = currentMillis;

    flowReadings[flowReadingsIndex] = flowMilliLitresPerMinute;
    flowReadingsIndex = (flowReadingsIndex + 1) % MOVING_AVERAGE_PERIOD;

    float totalFlow = 0.0;
    for (int i = 0; i < MOVING_AVERAGE_PERIOD; i++) {
      totalFlow += flowReadings[i];
    }
    smoothedFlowRate = totalFlow / MOVING_AVERAGE_PERIOD;

    Blynk.virtualWrite(V32, smoothedFlowRate); 
  }
}

void checkLeakageLevel() {  
    if (flowRateDifference < 0.08) {
        Blynk.virtualWrite(V35, "Negligible leakage");
    } else if (flowRateDifference < 0.4) {
        Blynk.virtualWrite(V35, "Low-level Leakage");
    } else if (flowRateDifference < 1.6) {
        Blynk.virtualWrite(V35, "Moderate Leakage");
    } else if (flowRateDifference < 4) {
        Blynk.virtualWrite(V35, "Substantial Leakage");
    } else {
        Blynk.virtualWrite(V35, "Severe Leakage");
    }
}

void checkSensor() {
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastSensorCheck > sensorCheckInterval) {
    // Read data from sensor
    dataFromSensor = pulseCount > 0;  // update this based on your specific sensor's data reading method
    if (dataFromSensor) {
      Blynk.virtualWrite(V38, "Online");
    } else {
      Blynk.virtualWrite(V38, "Offline");
    }
    lastSensorCheck = currentMillis;
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

  ArduinoOTA.begin();

  Blynk.begin(auth, ssid, password);

  timer.setInterval(1000L, checkForLeaks);
  timer.setInterval(5000L, checkSensor);  // new timer for checking sensor every 5 seconds
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

  Blynk.run();
  timer.run(); 
  calculateFlow();
  if(leakDetected) {
    checkLeakageLevel();
  }
}
