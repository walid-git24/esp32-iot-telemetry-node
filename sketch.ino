/*
  IoT Telemetry Node: ESP32 Wi-Fi & Register Control
  Challenge #01 - Milestone Evaluation

  - Reads analog sensor (Potentiometer/LDR) on GPIO4
  - Connects to Wokwi's virtual Wi-Fi
  - Sends sensor value via HTTP GET every 3 seconds (non-blocking)
  - Controls status LED on GPIO2 using direct register writes
    (GPIO_OUT_W1TS_REG to set, GPIO_OUT_W1TC_REG to clear) instead of digitalWrite()
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include "soc/gpio_reg.h"   // Gives us GPIO_OUT_W1TS_REG / GPIO_OUT_W1TC_REG addresses

// ---------- Configuration ----------
const char* ssid     = "Wokwi-GUEST";
const char* password = "";

// IMPORTANT: GPIO4/GPIO15 are ADC2 pins, and ESP32's ADC2 cannot be read
// reliably while Wi-Fi is active (the Arduino-ESP32 core blocks it and
// analogRead() returns 0). We use GPIO34 instead, which is an ADC1 pin
// and works perfectly fine at the same time as Wi-Fi.
const int sensorPin = 34;  // Potentiometer / LDR analog input (ADC1 - WiFi safe)
const int ledPin    = 2;   // Status LED (controlled via registers)

const unsigned long SEND_INTERVAL_MS = 3000;  // send HTTP every 3 seconds
unsigned long lastSendTime = 0;

// ---------- Helper functions for register-level LED control ----------
void ledOn() {
  // Write 1 To Set: sets GPIO2's bit HIGH
  WRITE_PERI_REG(GPIO_OUT_W1TS_REG, (1 << ledPin));
}

void ledOff() {
  // Write 1 To Clear: sets GPIO2's bit LOW
  WRITE_PERI_REG(GPIO_OUT_W1TC_REG, (1 << ledPin));
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(ledPin, OUTPUT);
  ledOff(); // start with LED off

  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // 1) ALWAYS read the sensor every loop iteration.
  //    This is what keeps the reading "live" / responsive even while
  //    a Wi-Fi request is being prepared or sent.
  int sensorValue = analogRead(sensorPin);
  Serial.print("Sensor raw value (0-4095): ");
  Serial.println(sensorValue);

  // 2) Non-blocking timer: only fire the HTTP request every 3 seconds,
  //    WITHOUT calling delay(3000) which would freeze analogRead().
  unsigned long now = millis();
  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = now;

    ledOn(); // blink LED to show a transmission is happening

    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = "http://httpbin.org/get?sensor=" + String(sensorValue);

      Serial.print("Sending GET: ");
      Serial.println(url);

      http.begin(url);
      int httpCode = http.GET();

      if (httpCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpCode);
      } else {
        Serial.print("HTTP request failed, error: ");
        Serial.println(http.errorToString(httpCode));
      }
      http.end();
    } else {
      Serial.println("WiFi not connected, skipping send.");
    }

    ledOff();
  }

  // Small delay just to keep Serial output readable;
  // 50ms is short enough that the potentiometer still feels instantly responsive.
  delay(50);
}
