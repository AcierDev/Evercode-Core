/**
 * NetworkComm ESP-NOW Send Status Callback - Receiver Board
 *
 * This example is a companion to the SendStatusCallback example.
 * It sets up a board that listens for and responds to pin control messages.
 *
 * Upload this sketch to a second ESP32 board to test message delivery
 * confirmation with the SendStatusCallback example.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "Everwood";
const char* password = "Everwood-Staff";

// Board configuration - must match with what the sender is expecting
const char* localBoardId = "receiver";  // This board's ID
const char* senderBoardId = "sender";   // The sender board's ID

// Pin definitions
const int ledPin = 13;     // The LED pin that will be controlled remotely
const int statusLed = 19;  // LED to indicate when messages are received

// NetworkComm instance
NetworkComm netComm;

// Handle pin control messages
void onPinChange(const char* sender, uint8_t pin, uint8_t value) {
  // Only accept control from known sender
  if (strcmp(sender, senderBoardId) == 0) {
    Serial.print("Received pin control from ");
    Serial.print(sender);
    Serial.print(": pin ");
    Serial.print(pin);
    Serial.print(" = ");
    Serial.println(value);

    // Set the pin as requested
    digitalWrite(pin, value);

    // Flash status LED to indicate message received
    digitalWrite(statusLed, HIGH);
    delay(50);
    digitalWrite(statusLed, LOW);
  } else {
    Serial.print("Received pin control from unknown sender: ");
    Serial.println(sender);
  }
}

// Handle board discovery
void onBoardDiscovered(const char* boardId) {
  Serial.print("Discovered board: ");
  Serial.println(boardId);

  if (strcmp(boardId, senderBoardId) == 0) {
    Serial.println("Sender board discovered!");

    // Flash status LED to show sender was found
    for (int i = 0; i < 3; i++) {
      digitalWrite(statusLed, HIGH);
      delay(100);
      digitalWrite(statusLed, LOW);
      delay(100);
    }
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nNetworkComm ESP-NOW Receiver Board");

  // Initialize pins
  pinMode(ledPin, OUTPUT);
  pinMode(statusLed, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(statusLed, LOW);

  // Initialize NetworkComm
  Serial.print("Connecting to WiFi...");
  if (netComm.begin(ssid, password, localBoardId)) {
    Serial.println("connected!");

    // Enable debug logging
    netComm.enableDebugLogging(true);

    // Register for pin control
    netComm.handlePinControl(onPinChange);

    // Register for discovery events
    netComm.onBoardDiscovered(onBoardDiscovered);

    // Signal successful connection
    digitalWrite(statusLed, HIGH);
    delay(500);
    digitalWrite(statusLed, LOW);

    Serial.println("Ready to receive pin control commands");
  } else {
    Serial.println("failed!");

    // Signal failed connection
    for (int i = 0; i < 5; i++) {
      digitalWrite(statusLed, HIGH);
      delay(100);
      digitalWrite(statusLed, LOW);
      delay(100);
    }
  }
}

void loop() {
  // Update NetworkComm to handle periodic tasks
  netComm.update();

  // Periodically report status
  static unsigned long lastStatusTime = 0;
  if (millis() - lastStatusTime > 10000) {  // Every 10 seconds
    lastStatusTime = millis();

    Serial.println("\n----- Receiver Status -----");
    Serial.print("LED state: ");
    Serial.println(digitalRead(ledPin) ? "ON" : "OFF");

    Serial.print("Connected to WiFi: ");
    Serial.println(netComm.isConnected() ? "YES" : "NO");

    Serial.print("Detected sender board: ");
    Serial.println(netComm.isBoardAvailable(senderBoardId) ? "YES" : "NO");

    Serial.print("Total boards available: ");
    Serial.println(netComm.getAvailableBoardsCount());
    Serial.println("-------------------------\n");
  }
}