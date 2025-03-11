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
const char* ssid = "Everwood";
const char* password = "Everwood-Staff";

// Board configuration
const char* localBoardId = "peripheral";   // This board
const char* remoteBoardId = "controller";  // Remote controller board

// Pin definitions - Using ESP32 compatible GPIO pins
const int ledPin1 = 13;  // LED 1 controlled by remote board
const int ledPin2 = 12;  // LED 2 controlled by remote board
const int ledPin3 = 14;  // LED 3 controlled by remote board (changed from 11
                         // which might not be available)

// NetworkComm instance
NetworkComm netComm;

// Callback for handling pin control messages
void onPinControlReceived(const char* sender, uint8_t pin, uint8_t value) {
  // Log the pin control message
  Serial.print("Pin control from ");
  Serial.print(sender);
  Serial.print(": Pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.println(value);

  // Update the physical pin based on the received command
  if (pin == ledPin1) {
    digitalWrite(ledPin1, value);
    Serial.print("Setting LED 1 to ");
    Serial.println(value ? "ON" : "OFF");
  } else if (pin == ledPin2) {
    digitalWrite(ledPin2, value);
    Serial.print("Setting LED 2 to ");
    Serial.println(value ? "ON" : "OFF");
  } else if (pin == ledPin3) {
    digitalWrite(ledPin3, value);
    Serial.print("Setting LED 3 to ");
    Serial.println(value ? "ON" : "OFF");
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);  // Give serial monitor time to connect
  Serial.println("NetworkComm Remote Pin Peripheral Example");

  // Initialize pins
  pinMode(ledPin1, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(ledPin3, OUTPUT);

  // Initialize NetworkComm with better error handling
  Serial.print("Connecting to WiFi...");
  int attempts = 0;
  bool connected = false;

  while (!connected && attempts < 20) {
    connected = netComm.begin(ssid, password, localBoardId);
    if (!connected) {
      Serial.print(".");
      delay(500);
      yield();  // Feed the watchdog
      attempts++;
    }
  }

  if (connected) {
    Serial.println("\nConnected to WiFi successfully");

    // Enable debug logging
    netComm.enableDebugLogging(true);

    // Set up callback to handle incoming pin control messages
    netComm.subscribeToPinChange(localBoardId, ledPin1, onPinControlReceived);
    netComm.subscribeToPinChange(localBoardId, ledPin2, onPinControlReceived);
    netComm.subscribeToPinChange(localBoardId, ledPin3, onPinControlReceived);

  } else {
    Serial.println("\nFailed to connect to WiFi after multiple attempts");
    Serial.println(
        "Continuing without WiFi - you may need to reset the device");
    // Don't get stuck in an infinite loop - continue anyway
  }

  Serial.println("Setup complete");
  Serial.println("Waiting for pin control messages...");
}

void loop() {
  // Feed the watchdog
  yield();

  // Update NetworkComm with safety check
  if (netComm.isConnected()) {
    netComm.update();
  }

  // Publish the current state of the LEDs every 5 seconds
  static unsigned long lastPublishTime = 0;
  if (netComm.isConnected() && millis() - lastPublishTime > 5000) {
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

  // Feed the watchdog again
  yield();

  // Small delay
  delay(10);
}