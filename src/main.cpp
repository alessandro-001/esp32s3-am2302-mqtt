#include <Arduino.h>    

#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Adafruit_NeoPixel.h>

// ─── WiFi ────────────────────────────────────────────────
#define WIFI_SSID     "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// ─── ThingsBoard ─────────────────────────────────────────
#define TB_HOST       "mqtt.thingsboard.cloud"
#define TB_PORT       1883
#define TB_TOKEN      "YOUR_THINGSBOARD_TOKEN_HERE"
#define TB_TOPIC      "v1/devices/me/telemetry"

// ─── AM2302 sensor ───────────────────────────────────────
#define DHTPIN        4
#define DHTTYPE       DHT22

// ─── Onboard RGB LED ─────────────────────────────────────
#define RGB_PIN       48
#define NUM_PIXELS    1

// ─── How often to send data (milliseconds) ───────────────
#define SEND_INTERVAL 5000

// ─────────────────────────────────────────────────────────

DHT dht(DHTPIN, DHTTYPE);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel led(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

unsigned long lastSendTime = 0;

void setLEDForTemperature(float temp) {
  if (temp < 18.0) {
    led.setPixelColor(0, led.Color(0, 0, 255));    // BLUE  — cold
  } else if (temp < 26.0) {
    led.setPixelColor(0, led.Color(0, 255, 0));    // GREEN — comfortable
  } else {
    led.setPixelColor(0, led.Color(255, 0, 0));    // RED   — hot
  }
  led.show();
}

void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  led.setPixelColor(0, led.Color(255, 165, 0));
  led.show();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  mqttClient.setServer(TB_HOST, TB_PORT);

  while (!mqttClient.connected()) {
    Serial.print("Connecting to ThingsBoard...");

    if (mqttClient.connect("ESP32S3_Client", TB_TOKEN, NULL)) {
      Serial.println("Connected to ThingsBoard!");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" — retrying in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  led.begin();
  led.setBrightness(10);
  led.clear();
  led.show();

  dht.begin();
  connectWiFi();
  connectMQTT();

  Serial.println("Ready... sending sensor data to ThingsBoard");
}

void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastSendTime >= SEND_INTERVAL) {
    lastSendTime = now;

    float humidity    = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("Failed to read from AM2302!");
      return;
    }

    char payload[64];
    snprintf(payload, sizeof(payload),
             "{\"temperature\":%.1f,\"humidity\":%.1f}",
             temperature, humidity);

    if (mqttClient.publish(TB_TOPIC, payload)) {
      Serial.print(">>>> Sent >>>> : ");
      Serial.println(payload);
    } else {
      Serial.println("Publish failed!");
    }

    setLEDForTemperature(temperature);
  }
}