/**
 * NetworkComm Pub-Sub Example
 *
 * This example demonstrates how to use the NetworkComm library
 * for publish-subscribe messaging between ESP32 boards.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Board configuration
const char* boardId = "pubsub-example";

// Topics
const char* temperatureTopic = "sensors/temperature";
const char* humidityTopic = "sensors/humidity";
const char* lightTopic = "sensors/light";
const char* controlTopic = "control/commands";

// Pin for simulated sensor readings
const int sensorPin = 34;  // ESP32 ADC pin

// NetworkComm instance
NetworkComm netComm;

// Callback for temperature messages
void onTemperatureMessage(const char* sender, const char* topic,
                          const char* message) {
  Serial.print("Temperature from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(message);
}

// Callback for humidity messages
void onHumidityMessage(const char* sender, const char* topic,
                       const char* message) {
  Serial.print("Humidity from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(message);
}

// Callback for light messages
void onLightMessage(const char* sender, const char* topic,
                    const char* message) {
  Serial.print("Light level from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(message);
}

// Callback for control messages
void onControlMessage(const char* sender, const char* topic,
                      const char* message) {
  Serial.print("Control command from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(message);

  // Process control commands
  if (strcmp(message, "start") == 0) {
    Serial.println("Starting sensor readings...");
  } else if (strcmp(message, "stop") == 0) {
    Serial.println("Stopping sensor readings...");
  } else if (strcmp(message, "reset") == 0) {
    Serial.println("Resetting...");
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("NetworkComm Pub-Sub Example");

  // Initialize analog pin
  pinMode(sensorPin, INPUT);

  // Initialize NetworkComm
  if (netComm.begin(ssid, password, boardId)) {
    Serial.println("Connected to WiFi and MQTT broker");
  } else {
    Serial.println("Failed to connect");
    while (1) {
      delay(1000);
    }
  }

  // Subscribe to topics
  netComm.subscribe(temperatureTopic, onTemperatureMessage);
  netComm.subscribe(humidityTopic, onHumidityMessage);
  netComm.subscribe(lightTopic, onLightMessage);
  netComm.subscribe(controlTopic, onControlMessage);

  Serial.println("Setup complete");
  Serial.println("Subscribed to topics:");
  Serial.println(temperatureTopic);
  Serial.println(humidityTopic);
  Serial.println(lightTopic);
  Serial.println(controlTopic);
}

void loop() {
  // Update NetworkComm
  netComm.update();

  // Simulate sensor readings and publish them
  static unsigned long lastTemperatureTime = 0;
  static unsigned long lastHumidityTime = 0;
  static unsigned long lastLightTime = 0;

  // Publish temperature every 5 seconds
  if (millis() - lastTemperatureTime > 5000) {
    lastTemperatureTime = millis();

    // Simulate temperature reading (20-30°C)
    float temperature = 20.0 + (random(0, 100) / 10.0);
    char tempStr[10];
    dtostrf(temperature, 4, 1, tempStr);

    // Publish temperature
    netComm.publish(temperatureTopic, tempStr);

    Serial.print("Published temperature: ");
    Serial.println(tempStr);
  }

  // Publish humidity every 7 seconds
  if (millis() - lastHumidityTime > 7000) {
    lastHumidityTime = millis();

    // Simulate humidity reading (30-70%)
    int humidity = 30 + random(0, 41);
    char humStr[10];
    sprintf(humStr, "%d%%", humidity);

    // Publish humidity
    netComm.publish(humidityTopic, humStr);

    Serial.print("Published humidity: ");
    Serial.println(humStr);
  }

  // Publish light level every 10 seconds
  if (millis() - lastLightTime > 10000) {
    lastLightTime = millis();

    // Read analog value from ESP32 ADC
    int lightLevel =
        analogRead(sensorPin) / 16;  // ESP32 has 12-bit ADC (0-4095)

    // Scale to 0-100%
    int lightPercent = map(lightLevel, 0, 255, 0, 100);
    char lightStr[10];
    sprintf(lightStr, "%d%%", lightPercent);

    // Publish light level
    netComm.publish(lightTopic, lightStr);

    Serial.print("Published light level: ");
    Serial.println(lightStr);
  }

  // Check for serial input to send control commands
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    // Publish control command
    netComm.publish(controlTopic, input.c_str());

    Serial.print("Published control command: ");
    Serial.println(input);
  }

  // Small delay
  delay(10);
}