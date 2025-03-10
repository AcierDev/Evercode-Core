/**
 * NetworkComm Library Example
 *
 * This example demonstrates how to use the NetworkComm library
 * for communication between ESP32 boards.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Board ID
const char* boardId = "board1";

// Pin definitions
const int ledPin = 13;
const int buttonPin = 2;

// NetworkComm instance
NetworkComm netComm;

// Last button state
int lastButtonState = HIGH;

// Callback for received messages
void onMessageReceived(const char* sender, const char* topic,
                       const char* message) {
  Serial.print("Message from ");
  Serial.print(sender);
  Serial.print(" on topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);
}

// Callback for pin changes
void onPinChanged(const char* sender, uint8_t pin, uint8_t value) {
  Serial.print("Pin change from ");
  Serial.print(sender);
  Serial.print(": Pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.println(value);

  // If it's a message about the LED pin, update our LED
  if (pin == ledPin) {
    digitalWrite(ledPin, value);
  }
}

// Callback for serial data
void onSerialData(const char* sender, const char* data) {
  Serial.print("Serial data from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(data);
}

// Callback for direct messages
void onDirectMessage(const char* sender, const char* topic,
                     const char* message) {
  Serial.print("Direct message from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(message);
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("NetworkComm Example");

  // Initialize pins
  pinMode(ledPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  // Initialize NetworkComm
  if (netComm.begin(ssid, password, boardId)) {
    Serial.println("Connected to WiFi and MQTT broker");
  } else {
    Serial.println("Failed to connect");
    while (1) {
      delay(1000);
    }
  }

  // Subscribe to messages
  netComm.subscribe("test/topic", onMessageReceived);

  // Subscribe to pin changes on another board
  netComm.subscribeToPinChange("board2", ledPin, onPinChanged);

  // Subscribe to serial data
  netComm.subscribeToSerialData(onSerialData);

  // Set callback for direct messages
  netComm.setDirectMessageCallback(onDirectMessage);

  Serial.println("Setup complete");
}

void loop() {
  // Update NetworkComm
  netComm.update();

  // Check button state
  int buttonState = digitalRead(buttonPin);

  // If button state changed
  if (buttonState != lastButtonState) {
    lastButtonState = buttonState;

    // Publish button state
    if (buttonState == LOW) {
      // Button pressed
      netComm.publish("test/topic", "Button pressed");

      // Set LED on another board
      netComm.setPinValue("board2", ledPin, HIGH);

      // Send direct message
      netComm.sendDirectMessage("board2", "Hello from board1");
    } else {
      // Button released
      netComm.publish("test/topic", "Button released");

      // Set LED on another board
      netComm.setPinValue("board2", ledPin, LOW);
    }
  }

  // Check for serial input
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');

    // Publish serial data
    netComm.publishSerialData(input.c_str());
  }

  // Small delay
  delay(10);
}