/**
 * NetworkComm Remote Pin Peripheral Example
 *
 * This example demonstrates how to use the NetworkComm library
 * to allow remote control of pins on this ESP32 board.
 * This is the peripheral board that will be controlled by
 * the RemotePinControl example.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Board configuration
const char* localBoardId = "peripheral";   // This board
const char* remoteBoardId = "controller";  // Remote controller board

// Pin definitions
const int ledPin1 = 13;  // LED 1 controlled by remote board
const int ledPin2 = 12;  // LED 2 controlled by remote board
const int ledPin3 = 11;  // LED 3 controlled by remote board

// NetworkComm instance
NetworkComm netComm;

// Callback for pin control messages
void onPinControlMessage(const char* topic, byte* payload,
                         unsigned int length) {
  // This is handled internally by the NetworkComm library
  // We just need to make sure update() is called regularly
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("NetworkComm Remote Pin Peripheral Example");

  // Initialize pins
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);

  // Initialize NetworkComm
  if (netComm.begin(ssid, password, localBoardId)) {
    Serial.println("Connected to WiFi and MQTT broker");
  } else {
    Serial.println("Failed to connect");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("Setup complete");
  Serial.println("Waiting for pin control messages...");
}

void loop() {
  // Update NetworkComm - this handles incoming pin control messages
  netComm.update();

  // Publish the current state of the LEDs every 5 seconds
  static unsigned long lastPublishTime = 0;
  if (millis() - lastPublishTime > 5000) {
    lastPublishTime = millis();

    // Read the current state of the LEDs
    int led1State = digitalRead(ledPin1);
    int led2State = digitalRead(ledPin2);
    int led3State = digitalRead(ledPin3);

    // Publish the state
    String message = "LED states: ";
    message += led1State ? "ON" : "OFF";
    message += ", ";
    message += led2State ? "ON" : "OFF";
    message += ", ";
    message += led3State ? "ON" : "OFF";

    netComm.publish("peripheral/status", message.c_str());

    Serial.println(message);
  }

  // Small delay
  delay(10);
}