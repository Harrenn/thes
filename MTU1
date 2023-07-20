//latest mtu1 code

#include <BlynkSimpleEsp32.h>
#include <WiFi.h>
#include <time.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#define SENSOR  4  // Change here
#define BLYNK_PRINT Serial
#define WIFI_SSID "HUAWEI-4049"
#define WIFI_PASS "personalwifisss1"

char auth[] = "_rT-A_f1jKTxycyowJ-kkFkVaieceJN2"; // Your Blynk auth token

volatile byte pulseCount;
byte pulse1Sec = 0;
float calibrationFactor = 4.5;
float flowMilliLitresPerMinute;  // Changed to float
float totalCubicMeter;  // Changed to float
unsigned long previousMillis = 0;

BlynkTimer timer;
WiFiServer telnetServer(23);
WiFiClient telnetClient;

void IRAM_ATTR pulseCounter()
{
  pulseCount++;
}

void telnetPrint(const char *s) {
  if (telnetClient.connected()) {
    telnetClient.print(s);
  }
}

void telnetPrintln(const char *s) {
  if (telnetClient.connected()) {
    telnetClient.println(s);
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
    Blynk.virtualWrite(V28, totalCubicMeter); // send totalCubicMeter to Blynk app in m^3

    telnetPrint("Flow rate: ");
    telnetPrint(String(flowMilliLitresPerMinute, 2).c_str());  // Prints the flowRate in mL/min to 2 decimal places
    telnetPrintln(" mL/min");

    telnetPrint("Total water consumed this month: ");
    telnetPrint(String(totalCubicMeter, 4).c_str()); // Prints the totalCubicMeter in m^3 to 4 decimal places
    telnetPrintln(" m^3");

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      // check if the day has changed to the first day of the month
      if (timeinfo.tm_mday == 1 && previousMillis != 1) {
        totalCubicMeter = 0.0; // reset the counter
        previousMillis = millis(); // update the previousMillis after reset
      }
      
      telnetPrint("Current date: ");
      telnetPrintln(asctime(&timeinfo)); // Print current date
    }
  }
}

void loop()
{
  Blynk.run();
  calculateFlow();

  ArduinoOTA.handle();

  if (!Blynk.connected()) {
    telnetPrintln("Lost connection to Blynk server...");
  }
  if (WiFi.status() != WL_CONNECTED) {
    telnetPrintln("Lost connection to WiFi...");
  }

  if (telnetServer.hasClient()) {
    if (telnetClient.connected()) {
      telnetClient.stop();
    }
    telnetClient = telnetServer.available();
  }
}


//latest code ++
