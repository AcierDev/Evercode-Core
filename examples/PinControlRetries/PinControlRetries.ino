/**
 * NetworkComm ESP-NOW Pin Control Retries Example
 *
 * This example demonstrates how to use the automatic retry functionality
 * for pin control messages. When enabled, pin control messages that fail
 * to deliver will be automatically retried up to the configured maximum
 * number of retries.
 *
 * This is particularly useful in environments with interference or when
 * controlling critical devices where message delivery is important.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Board configuration
const char* localBoardId = "controller";  // This board
const char* remoteBoardId = "responder";  // Remote board to control

// Define pins
const int buttonPin = 15;   // Button to control remote LED
const int localLed = 18;    // Local LED to show send status
const int successLed = 19;  // LED to show successful sends
const int failLed = 21;     // LED to show failed sends

// Remote pin definitions
const int remoteLedPin = 13;  // Remote LED pin

// Button state tracking
int lastButtonState = HIGH;

// Success/failure counters
int successCount = 0;
int failCount = 0;

// NetworkComm instance
NetworkComm netComm;

// Called for all message sends with the result (success or failure)
void onSendStatus(const char* targetBoardId, uint8_t messageType,
                  bool success) {
  // Turn on the appropriate LED
  digitalWrite(success ? successLed : failLed, HIGH);

  // Update counters
  if (success) {
    successCount++;
  } else {
    failCount++;
  }

  // Print status
  Serial.print("Message to ");
  Serial.print(targetBoardId);
  Serial.print(" ");
  Serial.println(success ? "SUCCEEDED" : "FAILED");

  // Turn off the LED after a short delay
  delay(50);
  digitalWrite(success ? successLed : failLed, LOW);
}

// Called specifically for pin control confirmations
void onPinControlConfirm(const char* sender, uint8_t pin, uint8_t value,
                         bool success) {
  Serial.print("Pin control to ");
  Serial.print(sender);
  Serial.print(" (pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.print(value);
  Serial.print(") ");
  Serial.println(success ? "SUCCEEDED" : "FAILED");

  // Show the final result on the local LED
  digitalWrite(localLed, success ? value : LOW);
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nNetworkComm Pin Control Retries Example");

  // Set up pins
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(localLed, OUTPUT);
  pinMode(successLed, OUTPUT);
  pinMode(failLed, OUTPUT);

  // Initialize NetworkComm
  if (netComm.begin(ssid, password, localBoardId)) {
    Serial.println("NetworkComm initialized successfully");
  } else {
    Serial.println("NetworkComm initialization failed");
    while (1) {
      digitalWrite(failLed, HIGH);
      delay(500);
      digitalWrite(failLed, LOW);
      delay(500);
    }
  }

  // Enable debug logging
  netComm.enableDebugLogging(true);

  // Register callbacks
  netComm.onSendStatus(onSendStatus);

  // Enable automatic retries for pin control messages
  netComm.enablePinControlRetries(true);

  // Configure retry parameters
  netComm.setPinControlMaxRetries(3);    // Maximum 3 retries
  netComm.setPinControlRetryDelay(500);  // 500ms between retries

  Serial.println("Automatic retries enabled with max 3 retries, 500ms delay");
  Serial.println("Press the button to toggle the remote LED");
}

void loop() {
  // Call NetworkComm update to process messages and retries
  netComm.update();

  // Read button state
  int buttonState = digitalRead(buttonPin);

  // Detect button press (falling edge)
  if (buttonState == LOW && lastButtonState == HIGH) {
    // Toggle the remote LED
    static bool ledState = false;
    ledState = !ledState;

    Serial.print("Sending pin control to toggle remote LED to ");
    Serial.println(ledState ? "ON" : "OFF");

    // Control the remote pin with confirmation callback
    netComm.controlRemotePin(remoteBoardId, remoteLedPin, ledState ? HIGH : LOW,
                             onPinControlConfirm);

    // Debounce
    delay(50);
  }

  // Update last button state
  lastButtonState = buttonState;

  // Print statistics every 5 seconds
  static unsigned long lastStatTime = 0;
  if (millis() - lastStatTime > 5000) {
    Serial.print("Statistics - Success: ");
    Serial.print(successCount);
    Serial.print(", Failures: ");
    Serial.println(failCount);
    lastStatTime = millis();
  }
}