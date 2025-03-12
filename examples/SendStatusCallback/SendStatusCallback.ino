/**
 * NetworkComm ESP-NOW Send Status Callback Example
 *
 * This example demonstrates how to use the ESP-NOW send status callback
 * feature to get real-time feedback on whether messages were successfully
 * delivered to remote boards.
 *
 * The ESP-NOW protocol provides immediate feedback on packet delivery at
 * the MAC layer, which is now exposed through this API.
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "Everwood";
const char* password = "Everwood-Staff";

// Board configuration
const char* localBoardId = "sender";     // This board
const char* remoteBoardId = "receiver";  // Remote board to control

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
  // Log the status
  Serial.print("Send status for message to ");
  Serial.print(targetBoardId);
  Serial.print(", type: ");
  Serial.print(messageType);
  Serial.print(": ");
  Serial.println(success ? "SUCCESS" : "FAILED");

  // Update status LEDs and counters
  if (success) {
    digitalWrite(successLed, HIGH);
    delay(100);  // Brief flash
    digitalWrite(successLed, LOW);
    successCount++;
  } else {
    // For failures, we'll let the dedicated failure callback handle the LED
    failCount++;
  }

  // Print statistics
  Serial.print("Success rate: ");
  int total = successCount + failCount;
  if (total > 0) {
    float rate = (float)successCount / total * 100.0;
    Serial.print(rate);
    Serial.print("% (");
    Serial.print(successCount);
    Serial.print("/");
    Serial.print(total);
    Serial.println(")");
  } else {
    Serial.println("No data yet");
  }
}

// Called only for message delivery failures
void onSendFailure(const char* targetBoardId, uint8_t messageType, uint8_t pin,
                   uint8_t value) {
  Serial.print("!! FAILURE !! Message to ");
  Serial.print(targetBoardId);
  Serial.print(", type: ");
  Serial.print(messageType);

  // For pin control messages, show the pin and value
  if (messageType == MSG_TYPE_PIN_CONTROL ||
      messageType == MSG_TYPE_PIN_CONTROL_CONFIRM) {
    Serial.print(", pin: ");
    Serial.print(pin);
    Serial.print(", value: ");
    Serial.print(value);
  }
  Serial.println();

  // Flash the failure LED
  digitalWrite(failLed, HIGH);
  delay(300);  // Longer flash for failures to make them more visible
  digitalWrite(failLed, LOW);

  // Example of specific actions you might take on failure
  Serial.println(
      "Failure action: Could retry message or take alternative action");

  // If the target board isn't available, we might want to start discovery
  if (!netComm.isBoardAvailable(targetBoardId)) {
    Serial.println("Target board not available - might need to rediscover");
  }
}

// Pin control confirmation callback - this is different from the send status
// It shows whether the pin was actually set on the remote device
void onPinControlConfirm(const char* sender, uint8_t pin, uint8_t value,
                         bool success) {
  Serial.print("Pin control confirmation from ");
  Serial.print(sender);
  Serial.print(": pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.print(value);
  Serial.print(", status: ");
  Serial.println(success ? "SUCCESS" : "FAILED");
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);  // Give serial time to initialize
  Serial.println("\n\nNetworkComm ESP-NOW Send Status Example");

  // Initialize pins
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(localLed, OUTPUT);
  pinMode(successLed, OUTPUT);
  pinMode(failLed, OUTPUT);

  // Turn all LEDs off initially
  digitalWrite(localLed, LOW);
  digitalWrite(successLed, LOW);
  digitalWrite(failLed, LOW);

  // Initialize NetworkComm
  Serial.print("Connecting to WiFi...");
  if (netComm.begin(ssid, password, localBoardId)) {
    Serial.println("connected!");

    // Enable debug logging
    netComm.enableDebugLogging(true);

    // Register the callbacks
    netComm.onSendStatus(onSendStatus);    // Register general status callback
    netComm.onSendFailure(onSendFailure);  // Register failure-specific callback

    // Flash success LED to indicate successful connection
    for (int i = 0; i < 3; i++) {
      digitalWrite(successLed, HIGH);
      delay(100);
      digitalWrite(successLed, LOW);
      delay(100);
    }
  } else {
    Serial.println("failed!");

    // Flash fail LED to indicate failed connection
    for (int i = 0; i < 3; i++) {
      digitalWrite(failLed, HIGH);
      delay(100);
      digitalWrite(failLed, LOW);
      delay(100);
    }
  }

  Serial.println("Setup complete. Press the button to control the remote LED.");
  Serial.println("The success and fail LEDs will show the send status.");
}

void loop() {
  // Update NetworkComm to handle periodic tasks
  netComm.update();

  // Read the button state
  int buttonState = digitalRead(buttonPin);

  // Check if button state changed
  if (buttonState != lastButtonState) {
    lastButtonState = buttonState;

    // If button is pressed (LOW when pressed with INPUT_PULLUP)
    if (buttonState == LOW) {
      // Light up local LED to show button press
      digitalWrite(localLed, HIGH);

      // Try to control the remote LED
      // Method 1: Using the built-in send status callback
      netComm.controlRemotePin(remoteBoardId, remoteLedPin, HIGH);

      Serial.println("Button pressed - sending HIGH to remote LED");

      // Example of sending to a non-existent board to test failure callback
      if (random(10) == 0) {  // 10% chance to test failure handling
        Serial.println(
            "Testing failure handling by sending to non-existent board");
        netComm.controlRemotePin("non-existent", remoteLedPin, HIGH);
      }
    } else {
      // Turn off local LED when button is released
      digitalWrite(localLed, LOW);

      // Method 2: Using the built-in callback plus a specific operation
      // callback
      netComm.controlRemotePin(remoteBoardId, remoteLedPin, LOW,
                               onPinControlConfirm);

      Serial.println("Button released - sending LOW to remote LED");
    }
  }

  // Display stats periodically
  static unsigned long lastStatsTime = 0;
  if (millis() - lastStatsTime > 10000) {  // Every 10 seconds
    lastStatsTime = millis();

    Serial.println("\n----- Send Status Statistics -----");
    Serial.print("Successful sends: ");
    Serial.println(successCount);
    Serial.print("Failed sends: ");
    Serial.println(failCount);

    int total = successCount + failCount;
    if (total > 0) {
      float rate = (float)successCount / total * 100.0;
      Serial.print("Success rate: ");
      Serial.print(rate);
      Serial.println("%");
    }
    Serial.println("--------------------------------\n");
  }
}