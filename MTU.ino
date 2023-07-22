#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <PubSubClient.h>
#include <time.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define SENSOR  4  
#define BLYNK_PRINT Serial
#define WIFI_SSID "HUAWEI-4049"
#define WIFI_PASS "personalwifisss1"

const char* mqtt_server = "20.163.192.238";
const int mqtt_port = 1883;

char auth[] = "_rT-A_f1jKTxycyowJ-kkFkVaieceJN2"; 

volatile byte pulseCount;
byte pulse1Sec = 0;
float calibrationFactor = 6.9;
float flowMilliLitresPerSecond;
float totalCubicMeter;
unsigned long previousMillis = 0;

int batteryLevel = 100;

BlynkTimer timer;
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void sendBatteryLevel() {
  Blynk.virtualWrite(V0, batteryLevel);
}

void decreaseBatteryLevel() {
  if (batteryLevel > 0) {
    batteryLevel--;
  }
}

void setup_mqtt() {
  mqttClient.setServer(mqtt_server, mqtt_port);
  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT...");

    if (mqttClient.connect("ESP32Client")) {
      Serial.println("connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(mqttClient.state());
      delay(2000);
    }
  }
}

BLYNK_WRITE(V33) {
  ESP.restart();
}

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    ESP.restart();
  }
  
  ArduinoOTA.begin();
  Blynk.begin(auth, WIFI_SSID, WIFI_PASS);
  setup_mqtt();
  
  Blynk.syncVirtual(V30);
  Blynk.syncVirtual(V0);

  pinMode(SENSOR, INPUT_PULLUP);
  pulseCount = 0;
  flowMilliLitresPerSecond = 0.0;
  totalCubicMeter = 0.0;
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
  
  configTime(8 * 3600, 0, "pool.ntp.org");

  previousMillis = millis();
  timer.setInterval(1000L, sendBatteryLevel);
  timer.setInterval(300000L, decreaseBatteryLevel);
}

BLYNK_WRITE(V30)
{
  totalCubicMeter = param.asFloat(); 
}

BLYNK_WRITE(V0)
{
  batteryLevel = param.asInt(); 
}

void calculateFlow()
{
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis > 1000) 
  {
    pulse1Sec = pulseCount;
    pulseCount = 0;
    float flowRateLperMin = ((1000.0 / (currentMillis - previousMillis)) * pulse1Sec) / calibrationFactor;
    flowMilliLitresPerSecond = (flowRateLperMin * 1000) / 60;
    totalCubicMeter += flowMilliLitresPerSecond / (1000 * 1000);

    previousMillis = currentMillis;
    Blynk.virtualWrite(V27, flowMilliLitresPerSecond);
    Blynk.virtualWrite(V30, totalCubicMeter);

    mqttClient.publish("flowRate", String(flowMilliLitresPerSecond, 2).c_str());
  }
}

void loop()
{
  Blynk.run();
  ArduinoOTA.handle();
  timer.run();
  calculateFlow();
}
