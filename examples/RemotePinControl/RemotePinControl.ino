/**
 * NetworkComm Remote Pin Control Example
 *
 * This example demonstrates how to use the NetworkComm library
 * to remotely control pins on another ESP32 board.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "Everwood";
const char* password = "Everwood-Staff";

// Board configuration
const char* localBoardId = "controller";   // This board
const char* remoteBoardId = "peripheral";  // Remote board to control

// Pin definitions - ESP32 compatible GPIO pins
const int buttonPin1 = 15;  // Button to control remote LED 1 (changed from 2)
const int buttonPin2 = 16;  // Button to control remote LED 2 (changed from 3)
const int buttonPin3 = 17;  // Button to control remote LED 3 (changed from 4)
const int ledPin1 = 18;     // Local LED 1 (changed from 5)
const int ledPin2 = 19;     // Local LED 2 (changed from 6)
const int ledPin3 = 21;     // Local LED 3 (changed from 7)

// Remote pin definitions
const int remoteLedPin1 = 13;  // Remote LED 1
const int remoteLedPin2 = 12;  // Remote LED 2
const int remoteLedPin3 =
    14;  // Remote LED 3 - updated to match peripheral board

// Button states
int lastButtonState1 = HIGH;
int lastButtonState2 = HIGH;
int lastButtonState3 = HIGH;

// Board discovery tracking
bool peripheralDiscovered = false;

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

// Called when another board is discovered
void onBoardDiscovered(const char* boardId) {
  // Only process discovery if we haven't already discovered this board
  if (!peripheralDiscovered && strcmp(boardId, remoteBoardId) == 0) {
    Serial.print("Discovered board: ");
    Serial.println(boardId);

    peripheralDiscovered = true;

    // Now that we found the peripheral, subscribe to pin changes
    Serial.println(
        "Peripheral board discovered. Subscribing to pin changes...");
    netComm.subscribeToPinChange(remoteBoardId, remoteLedPin1, onPinChanged);
    netComm.subscribeToPinChange(remoteBoardId, remoteLedPin2, onPinChanged);
    netComm.subscribeToPinChange(remoteBoardId, remoteLedPin3, onPinChanged);
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);  // Give serial time to initialize
  Serial.println("NetworkComm Remote Pin Control Example");

  // Initialize pins
  pinMode(buttonPin1, INPUT_PULLUP);
  pinMode(buttonPin2, INPUT_PULLUP);
  pinMode(buttonPin3, INPUT_PULLUP);
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

    // Set up a callback for when a board is discovered
    netComm.setDiscoveryCallback(onBoardDiscovered);

    // Wait a moment for initial broadcasts to be processed
    delay(1000);

    Serial.println("Waiting for peripheral board to be discovered...");
    // Subscriptions will happen in the discovery callback now

  } else {
    Serial.println("\nFailed to connect to WiFi after multiple attempts");
    Serial.println(
        "Continuing without WiFi - you may need to reset the device");
    // Don't get stuck in an infinite loop - continue anyway
  }

  Serial.println("Setup complete");
}

void loop() {
  // Feed the watchdog
  yield();

  // Update NetworkComm with error handling
  if (netComm.isConnected()) {
    netComm.update();
  }

  // Check button 1 state
  int buttonState1 = digitalRead(buttonPin1);
  if (buttonState1 != lastButtonState1) {
    lastButtonState1 = buttonState1;

    // Set remote LED 1 - only if peripheral has been discovered
    if (buttonState1 == LOW && netComm.isConnected()) {
      if (peripheralDiscovered) {
        // Button pressed
        netComm.setPinValue(remoteBoardId, remoteLedPin1, HIGH);
        Serial.println("Button 1 pressed, turning remote LED 1 ON");
      } else {
        Serial.println("Button 1 pressed, but peripheral not yet discovered");
      }
    } else if (netComm.isConnected()) {
      if (peripheralDiscovered) {
        // Button released
        netComm.setPinValue(remoteBoardId, remoteLedPin1, LOW);
        Serial.println("Button 1 released, turning remote LED 1 OFF");
      } else {
        Serial.println("Button 1 released, but peripheral not yet discovered");
      }
    }
  }

  // Check button 2 state
  int buttonState2 = digitalRead(buttonPin2);
  if (buttonState2 != lastButtonState2) {
    lastButtonState2 = buttonState2;

    // Set remote LED 2 - only if peripheral has been discovered
    if (buttonState2 == LOW && netComm.isConnected()) {
      if (peripheralDiscovered) {
        // Button pressed
        netComm.setPinValue(remoteBoardId, remoteLedPin2, HIGH);
        Serial.println("Button 2 pressed, turning remote LED 2 ON");
      } else {
        Serial.println("Button 2 pressed, but peripheral not yet discovered");
      }
    } else if (netComm.isConnected()) {
      if (peripheralDiscovered) {
        // Button released
        netComm.setPinValue(remoteBoardId, remoteLedPin2, LOW);
        Serial.println("Button 2 released, turning remote LED 2 OFF");
      } else {
        Serial.println("Button 2 released, but peripheral not yet discovered");
      }
    }
  }

  // Check button 3 state
  int buttonState3 = digitalRead(buttonPin3);
  if (buttonState3 != lastButtonState3) {
    lastButtonState3 = buttonState3;

    // Set remote LED 3 - only if peripheral has been discovered
    if (buttonState3 == LOW && netComm.isConnected()) {
      if (peripheralDiscovered) {
        // Button pressed
        netComm.setPinValue(remoteBoardId, remoteLedPin3, HIGH);
        Serial.println("Button 3 pressed, turning remote LED 3 ON");
      } else {
        Serial.println("Button 3 pressed, but peripheral not yet discovered");
      }
    } else if (netComm.isConnected()) {
      if (peripheralDiscovered) {
        // Button released
        netComm.setPinValue(remoteBoardId, remoteLedPin3, LOW);
        Serial.println("Button 3 released, turning remote LED 3 OFF");
      } else {
        Serial.println("Button 3 released, but peripheral not yet discovered");
      }
    }
  }

  // Display discovery status periodically
  static unsigned long lastStatusTime = 0;
  if (!peripheralDiscovered && millis() - lastStatusTime > 5000) {
    lastStatusTime = millis();
    Serial.println("Waiting for peripheral board to be discovered...");
  }

  // Feed the watchdog again
  yield();

  // Small delay
  delay(10);
}