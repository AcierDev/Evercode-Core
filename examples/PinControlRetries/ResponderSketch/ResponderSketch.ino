/**
 * NetworkComm ESP-NOW Pin Control Retries Example - Responder
 *
 * This sketch is the responder side of the Pin Control Retries example.
 * It simply listens for pin control messages and controls the LED accordingly.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Board configuration
const char* localBoardId = "responder";  // This board

// Define pins
const int ledPin = 13;  // LED to be controlled remotely

// NetworkComm instance
NetworkComm netComm;

// Called when a pin control message is received
void onPinControl(const char* sender, uint8_t pin, uint8_t value) {
  Serial.print("Received pin control from ");
  Serial.print(sender);
  Serial.print(": pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.println(value);

  // Control the LED
  if (pin == ledPin) {
    digitalWrite(ledPin, value);
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nNetworkComm Pin Control Retries Example - Responder");

  // Set up pins
  pinMode(ledPin, OUTPUT);

  // Initialize NetworkComm
  if (netComm.begin(ssid, password, localBoardId)) {
    Serial.println("NetworkComm initialized successfully");
  } else {
    Serial.println("NetworkComm initialization failed");
    while (1) {
      digitalWrite(ledPin, HIGH);
      delay(100);
      digitalWrite(ledPin, LOW);
      delay(100);
    }
  }

  // Enable debug logging
  netComm.enableDebugLogging(true);

  // Set up pin control handling
  netComm.handlePinControl(onPinControl);

  Serial.println("Ready to receive pin control messages");
}

void loop() {
  // Call NetworkComm update to process messages
  netComm.update();
}