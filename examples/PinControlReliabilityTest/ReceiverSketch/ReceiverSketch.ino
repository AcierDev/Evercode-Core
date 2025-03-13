/**
 * NetworkComm ESP-NOW Pin Control Reliability Test - Receiver
 *
 * This sketch is the receiver side of the Pin Control Reliability Test.
 * It receives pin control messages, controls the LED accordingly,
 * and keeps track of how many messages it has received.
 *
 * This helps verify that the sender's statistics match what the receiver
 * actually received.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Board configuration
const char* localBoardId = "receiver";  // This board

// Define pins
const int ledPin = 13;    // LED to be controlled remotely
const int statusLed = 2;  // LED to show activity

// Statistics
int messagesReceived = 0;
int lastValue = -1;  // Track last value to detect changes

// NetworkComm instance
NetworkComm netComm;

// Called when a pin control message is received
void onPinControl(const char* sender, uint8_t pin, uint8_t value) {
  // Increment message counter
  messagesReceived++;

  // Control the LED
  if (pin == ledPin) {
    digitalWrite(ledPin, value);

    // Only count as a change if the value is different
    if (lastValue != value) {
      lastValue = value;

      // Visual indicator for value change
      digitalWrite(statusLed, HIGH);
      delay(10);
      digitalWrite(statusLed, LOW);
    }
  }

  // Print progress every 10 messages
  if (messagesReceived % 10 == 0) {
    Serial.print("Received ");
    Serial.print(messagesReceived);
    Serial.println(" messages");
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nNetworkComm Pin Control Reliability Test - Receiver");

  // Set up pins
  pinMode(ledPin, OUTPUT);
  pinMode(statusLed, OUTPUT);

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

  // Enable debug logging (optional, comment out for less verbose output)
  // netComm.enableDebugLogging(true);

  // Set up pin control handling
  netComm.handlePinControl(onPinControl);

  // Enable acknowledgements for more reliable delivery
  netComm.enableMessageAcknowledgements(true);

  Serial.println("Ready to receive pin control messages");

  // Indicate ready status
  for (int i = 0; i < 3; i++) {
    digitalWrite(statusLed, HIGH);
    delay(200);
    digitalWrite(statusLed, LOW);
    delay(200);
  }
}

void loop() {
  // Call NetworkComm update to process messages
  netComm.update();

  // Print statistics every 5 seconds
  static unsigned long lastStatTime = 0;
  if (millis() - lastStatTime > 5000) {
    Serial.print("Total messages received: ");
    Serial.println(messagesReceived);
    lastStatTime = millis();
  }
}