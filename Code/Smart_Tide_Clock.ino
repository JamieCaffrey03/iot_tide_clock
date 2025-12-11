#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include "Display_EPD_W21_spi.h"
#include "Display_EPD_W21.h"
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP 21600        
#define SSID ""
#define PASSPH ""
#define API_KEY ""

// MQTT Broker settings
const char* mqtt_server = "192.168.0.89";
const int mqtt_port = 1883;
const char* mqtt_user = "";
const char* mqtt_password = "";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

void connectToMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    mqttClient.loop();
    if (mqttClient.connect("TideClockClient", mqtt_user, mqtt_password)) {
      delay(100);
      Serial.println("connected!");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      mqttClient.loop();
      delay(500);
    }
  }
}

void publishTideEvent(String type, String time, float height) {
  StaticJsonDocument<256> doc;
  doc["type"] = type;
  doc["time"] = time;
  doc["height"] = height;

  String output;
  serializeJson(doc, output);

  Serial.print("MQTT Payload: ");
  Serial.println(output);

  bool success = mqttClient.publish("tideclock/next", output.c_str());
  Serial.print("MQTT publish success: ");
  Serial.println(success ? "YES" : "NO");
}

// location for Howth tide station
float latitude = 53.383;
float longitude = -6.067;

// Full-screen canvas buffer for drawing
GFXcanvas1 canvas(EPD_WIDTH, EPD_HEIGHT);

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSPH);
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setKeepAlive(15); // seconds

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  // Set pin modes
  pinMode(2, INPUT);   // BUSY
  pinMode(14, OUTPUT); // RES
  pinMode(6, OUTPUT);  // DC
  pinMode(1, OUTPUT);  // CS

  SPI.begin();  
  SPI.beginTransaction(SPISettings(10000000, MSBFIRST, SPI_MODE0));
  EPD_Init();

  // Configure the timer to wake up the ESP32 every 6 hours
  //esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}

void drawTitle() {
  canvas.setFont(&FreeSansBold24pt7b);
  canvas.setTextSize(1);
  canvas.setTextColor(1);
  canvas.setCursor(50, 60);
  canvas.print("TIDE PREDICTIONS");
  
  // Draw location
  canvas.setFont(&FreeSans18pt7b);
  canvas.setCursor(50, 100);
  canvas.print("Location: ");
  canvas.print(latitude, 2);
  canvas.print(", ");
  canvas.print(longitude, 2);
  canvas.print(" (Howth) ");
  
  // Draw divider line
  canvas.drawLine(0, 120, EPD_WIDTH, 120, 1);
}

void drawTideEvent(int row, String type, String time, float height) {
  int yPos = 180 + row * 80;
  
  // Draw icons for high and low tide
  if (type == "High") {
    canvas.fillTriangle(50, yPos - 20, 70, yPos, 30, yPos, 1); // Simple up arrow
  } else {
    canvas.fillTriangle(50, yPos, 70, yPos - 20, 30, yPos - 20, 1); // Simple down arrow
  }
  
  // Draw tide type
  canvas.setFont(&FreeSansBold24pt7b);
  canvas.setTextSize(1);
  canvas.setCursor(100, yPos);
  canvas.print(type);
  
  // Draw time
  canvas.setFont(&FreeSans18pt7b);
  canvas.setCursor(300, yPos);
  canvas.print(time);
  
  // Draw height
  canvas.setCursor(550, yPos);
  canvas.print(String(height, 2) + " m");
  
  // Draw divider line if not last row
  if (row < 3) {
    canvas.drawLine(0, yPos + 40, EPD_WIDTH, yPos + 40, 1);
  }
}

String adjustForBST(String isoTime) {
  int hour = isoTime.substring(11, 13).toInt();
  int minute = isoTime.substring(14, 16).toInt();

  // Adjust for BST (UTC+1)
  hour = (hour + 1) % 24;

  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
  return String(buffer);
}

void reconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    mqttClient.loop();
    if (mqttClient.connect("TideClockClient", mqtt_user, mqtt_password)) {
      delay(100);
      Serial.println("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("test/status", "ESP32 connected");
    } else {
      mqttClient.loop();
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
      mqttClient.loop();
    }
  }
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;

    String url = "http://www.worldtides.info/api/v3?extremes&lat=" + String(latitude, 6) +
                 "&lon=" + String(longitude, 6) + "&key=" + API_KEY;

    Serial.println(url);

    http.begin(client, url.c_str());
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("==== RAW JSON PAYLOAD ====");
      Serial.println(payload);

      StaticJsonDocument<4096> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("JSON deserialization failed: ");
        Serial.println(error.c_str());
        http.end();
        return;
      }

      if (!doc.containsKey("extremes")) {
        Serial.println("No 'extremes' data in response.");
        http.end();
        return;
      }

      // Clear screen
      canvas.fillScreen(0);
      
      // Draw title and header
      drawTitle();
      
      JsonArray extremes = doc["extremes"];
      Serial.print("Number of tide events: ");
      Serial.println(extremes.size());

      // Draw tide events
      for (int i = 0; i < 4 && i < extremes.size(); i++) {
        String type = extremes[i]["type"].as<String>();
        String dateTime = extremes[i]["date"].as<String>();
        float height = extremes[i]["height"].as<float>();
        // String timeOnly = dateTime.substring(11, 16); // gmt
        String timeOnly = adjustForBST(dateTime); // gmt + 1
        
        drawTideEvent(i, type, timeOnly, height);
      }

      if (!mqttClient.connected()) {
        reconnect();
      }
      mqttClient.loop();
        if (!mqttClient.connected()) {
        reconnect();
      }
      delay(500);
      mqttClient.loop();

      // Publish first tide event
      if (extremes.size() > 0) {
        mqttClient.loop();
        String type = extremes[0]["type"].as<String>();
        String dateTime = extremes[0]["date"].as<String>();
        float height = extremes[0]["height"].as<float>();
        String timeOnly = dateTime.substring(11, 16);
        delay(500);
        publishTideEvent(type, timeOnly, height);
        unsigned long start = millis();
        while (millis() - start < 2000) {
          mqttClient.loop();
        }
      }

      // Push buffer to screen
      EPD_WhiteScreen_ALL(canvas.getBuffer());
    } else {
      Serial.printf("HTTP request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    WiFi.begin(SSID, PASSPH);
  }

  delay(2000);
  
  // Serial.println("Entering deep sleep for 6 hours...");
  // esp_deep_sleep_start();

}
