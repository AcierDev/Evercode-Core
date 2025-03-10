/**
 * NetworkComm Remote Pin Control Example
 *
 * This example demonstrates how to use the NetworkComm library
 * to remotely control pins on another ESP32 board.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Board configuration
const char* localBoardId = "controller";   // This board
const char* remoteBoardId = "peripheral";  // Remote board to control

// Pin definitions
const int buttonPin1 = 2;  // Button to control remote LED 1
const int buttonPin2 = 3;  // Button to control remote LED 2
const int buttonPin3 = 4;  // Button to control remote LED 3
const int ledPin1 = 5;     // Local LED 1 (mirrors remote LED 1)
const int ledPin2 = 6;     // Local LED 2 (mirrors remote LED 2)
const int ledPin3 = 7;     // Local LED 3 (mirrors remote LED 3)

// Remote pin definitions
const int remoteLedPin1 = 13;  // Remote LED 1
const int remoteLedPin2 = 12;  // Remote LED 2
const int remoteLedPin3 = 11;  // Remote LED 3

// Button states
int lastButtonState1 = HIGH;
int lastButtonState2 = HIGH;
int lastButtonState3 = HIGH;

// NetworkComm instance
NetworkComm netComm;

// Callback for pin changes
void onPinChanged(const char* sender, uint8_t pin, uint8_t value) {
  Serial.print("Pin change from ");
  Serial.print(sender);
  Serial.print(": Pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.println(value);

  // Update local LEDs to mirror remote LEDs
  if (pin == remoteLedPin1) {
    digitalWrite(ledPin1, value);
  } else if (pin == remoteLedPin2) {
    digitalWrite(ledPin2, value);
  } else if (pin == remoteLedPin3) {
    digitalWrite(ledPin3, value);
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("NetworkComm Remote Pin Control Example");

  // Initialize pins
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);
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

  // Subscribe to pin changes on remote board
  netComm.subscribeToPinChange(remoteBoardId, remoteLedPin1, onPinChanged);
  netComm.subscribeToPinChange(remoteBoardId, remoteLedPin2, onPinChanged);
  netComm.subscribeToPinChange(remoteBoardId, remoteLedPin3, onPinChanged);

  Serial.println("Setup complete");
}

void loop() {
  // Update NetworkComm
  netComm.update();

  // Check button 1 state
  int buttonState1 = digitalRead(buttonPin1);
  if (buttonState1 != lastButtonState1) {
    lastButtonState1 = buttonState1;

    // Set remote LED 1
    if (buttonState1 == LOW) {
      // Button pressed
      netComm.setPinValue(remoteBoardId, remoteLedPin1, HIGH);
      Serial.println("Button 1 pressed, turning remote LED 1 ON");
    } else {
      // Button released
      netComm.setPinValue(remoteBoardId, remoteLedPin1, LOW);
      Serial.println("Button 1 released, turning remote LED 1 OFF");
    }
  }

  // Check button 2 state
  int buttonState2 = digitalRead(buttonPin2);
  if (buttonState2 != lastButtonState2) {
    lastButtonState2 = buttonState2;

    // Set remote LED 2
    if (buttonState2 == LOW) {
      // Button pressed
      netComm.setPinValue(remoteBoardId, remoteLedPin2, HIGH);
      Serial.println("Button 2 pressed, turning remote LED 2 ON");
    } else {
      // Button released
      netComm.setPinValue(remoteBoardId, remoteLedPin2, LOW);
      Serial.println("Button 2 released, turning remote LED 2 OFF");
    }
  }

  // Check button 3 state
  int buttonState3 = digitalRead(buttonPin3);
  if (buttonState3 != lastButtonState3) {
    lastButtonState3 = buttonState3;

    // Set remote LED 3
    if (buttonState3 == LOW) {
      // Button pressed
      netComm.setPinValue(remoteBoardId, remoteLedPin3, HIGH);
      Serial.println("Button 3 pressed, turning remote LED 3 ON");
    } else {
      // Button released
      netComm.setPinValue(remoteBoardId, remoteLedPin3, LOW);
      Serial.println("Button 3 released, turning remote LED 3 OFF");
    }
  }

  // Small delay
  delay(10);
}