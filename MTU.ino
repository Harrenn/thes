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
WiFiServer telnetServer(23);
WiFiClient espClient;
WiFiClient telnetWiFiClient;
PubSubClient mqttClient(espClient);

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void telnetPrint(const char *s) {
  if (telnetWiFiClient.connected()) {
    telnetWiFiClient.print(s);
  }
}

void telnetPrintln(const char *s) {
  if (telnetWiFiClient.connected()) {
    telnetWiFiClient.println(s);
  }
}

void sendBatteryLevel() {
  Blynk.virtualWrite(V0, batteryLevel);
}

void decreaseBatteryLevel() {
  if (batteryLevel > 0) {
    batteryLevel--;
  }
  telnetPrintln(("Current battery level: " + String(batteryLevel)).c_str());
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
  String input_text = param.asStr(); 
  Blynk.virtualWrite(V30, 0.0); 
  delay(1000);
  ESP.restart();
}

void setup()
{
  Serial.begin(115200);
  telnetPrintln("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    ESP.restart();
  }
  
  ArduinoOTA.begin();
  telnetServer.begin();
  Blynk.begin(auth, WIFI_SSID, WIFI_PASS);
  setup_mqtt();
  
  Blynk.syncVirtual(V30);
  Blynk.syncVirtual(V0);

  if (WiFi.status() == WL_CONNECTED) {
    telnetPrintln("Connected to WiFi!");
  } else {
    telnetPrintln("Failed to connect to WiFi...");
  }

  if (Blynk.connected()) {
    telnetPrintln("Connected to Blynk server!");
  } else {
    telnetPrintln("Failed to connect to Blynk server...");
  }

  pinMode(SENSOR, INPUT_PULLUP);
  pulseCount = 0;
  flowMilliLitresPerSecond = 0.0;
  totalCubicMeter = 0.0;
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
  
  configTime(8 * 3600, 0, "pool.ntp.org");

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    telnetPrintln(asctime(&timeinfo));
  }

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

    telnetPrint("Flow rate: ");
    telnetPrint(String(flowMilliLitresPerSecond, 2).c_str()); 
    telnetPrintln(" mL/s");
    telnetPrint("Total water consumed this month: ");
    telnetPrint(String(totalCubicMeter, 4).c_str()); 
    telnetPrintln(" m^3");

    mqttClient.publish("flowRate", String(flowMilliLitresPerSecond, 2).c_str());
  }
}

void loop()
{
  Blynk.run();
  ArduinoOTA.handle();
  timer.run();
  if (telnetServer.hasClient()) {
    if (telnetWiFiClient.connected()) {
      telnetWiFiClient.stop();
    }
    telnetWiFiClient = telnetServer.available();
  }
  calculateFlow();
}
