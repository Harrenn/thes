#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <time.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>

#define SENSOR  4  // Change here
#define BLYNK_PRINT Serial
#define WIFI_SSID "HUAWEI-4049"
#define WIFI_PASS "personalwifisss1"

const char* mqtt_server = "20.163.192.238";
const int mqtt_port = 1883;

char auth[] = "_rT-A_f1jKTxycyowJ-kkFkVaieceJN2"; // Your Blynk auth token

volatile byte pulseCount;
byte pulse1Sec = 0;
float calibrationFactor = 4.5;
float flowMilliLitresPerMinute;  // Changed to float
float totalCubicMeter;  // Changed to float
unsigned long previousMillis = 0;

int batteryLevel = 100; // Define battery level (0 - 100%)

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
  // Send battery level to virtual pin V0
  Blynk.virtualWrite(V0, batteryLevel);
}

void decreaseBatteryLevel() {
  // Decrease the battery level by 1
  if (batteryLevel > 0) {
    batteryLevel--;
  }

  // Print the current battery level
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

  setup_mqtt(); // set up MQTT
  
  Blynk.syncVirtual(V30);  // Add blynk sync command for totalCubicMeter
  Blynk.syncVirtual(V0);  // Add blynk sync command for batteryLevel

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
  flowMilliLitresPerMinute = 0.0;
  totalCubicMeter = 0.0;
  attachInterrupt(digitalPinToInterrupt(SENSOR), pulseCounter, FALLING);
  
  configTime(8 * 3600, 0, "pool.ntp.org"); // Set the timezone to GMT+8 (Philippines Standard Time)

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    telnetPrintln(asctime(&timeinfo)); // Print current date and time
  }

  previousMillis = millis();

  // Setup timers
  timer.setInterval(1000L, sendBatteryLevel); // Send battery level every 1 second
  timer.setInterval(300000L, decreaseBatteryLevel); // Decrease battery level every 5 minutes
}

// This function will be called every time App writes value to Virtual Pin 28
BLYNK_WRITE(V30)
{
  totalCubicMeter = param.asFloat(); // assigning incoming value from server to totalCubicMeter
}

// This function will be called every time App writes value to Virtual Pin 30
BLYNK_WRITE(V0)
{
  batteryLevel = param.asInt(); // assigning incoming value from server to batteryLevel
}

void calculateFlow()
{
  unsigned long currentMillis = millis();
  
  if (currentMillis - previousMillis > 1000) 
  {
    pulse1Sec = pulseCount;
    pulseCount = 0;

    float flowRateLperMin = ((1000.0 / (currentMillis - previousMillis)) * pulse1Sec) / calibrationFactor;
    flowMilliLitresPerMinute = flowRateLperMin * 1000; // L/min to mL/min
    totalCubicMeter += flowMilliLitresPerMinute / (1000 * 1000 * 60); // mL/min to m^3

    previousMillis = currentMillis;
    
    Blynk.virtualWrite(V27, flowMilliLitresPerMinute); // send flowRate to Blynk app in mL/min
    Blynk.virtualWrite(V30, totalCubicMeter); // send totalCubicMeter to Blynk app in m^3

    telnetPrint("Flow rate: ");
    telnetPrint(String(flowMilliLitresPerMinute, 2).c_str());  // Prints the flowRate in mL/min to 2 decimal places
    telnetPrintln(" mL/min");

    telnetPrint("Total water consumed this month: ");
    telnetPrint(String(totalCubicMeter, 4).c_str()); // Prints the totalCubicMeter in m^3 to 4 decimal places
    telnetPrintln(" m^3");

    mqttClient.publish("flowRate", String(flowMilliLitresPerMinute, 2).c_str()); // publish flow rate via MQTT

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      // check if the day has changed to the first day of the month
      if (timeinfo.tm_mday == 1 && previousMillis != 1) {
        totalCubicMeter = 0.0; // reset the counter
        previousMillis = millis(); // update the previousMillis after reset
        Blynk.virtualWrite(V30, totalCubicMeter); // update the value on the Blynk server
      }
      
      telnetPrint("Current date: ");
      telnetPrintln(asctime(&timeinfo)); // Print current date and time
    }
  }
}

void loop()
{
  Blynk.run();
  timer.run();
  calculateFlow();
  
  WiFiClient client = telnetServer.available();
  if (client) {
    if (!telnetWiFiClient || !telnetWiFiClient.connected()) {
      if (telnetWiFiClient) {
        telnetWiFiClient.stop();
      }
      telnetWiFiClient = client;
    }
  }
  
  ArduinoOTA.handle();

  // Check MQTT connection and reconnect if needed
  if (!mqttClient.connected()) {
    setup_mqtt();
  }
  mqttClient.loop();
}
