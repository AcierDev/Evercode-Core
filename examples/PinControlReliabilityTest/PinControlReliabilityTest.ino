/**
 * NetworkComm ESP-NOW Pin Control Reliability Test
 *
 * This example sends 500 pin control messages with retries enabled and
 * tracks how many successfully make it through. This is useful for testing
 * the reliability of the automatic retry functionality in different
 * environments or with different retry settings.
 *
 * The test alternates between HIGH and LOW values for the remote pin,
 * and each message has a unique sequence number for tracking.
 *
 * Results are displayed on the serial monitor, showing:
 * - Total messages sent
 * - Messages delivered successfully on first attempt
 * - Messages delivered after retries
 * - Messages that failed despite retries
 * - Overall success rate
 */

#include <Arduino.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Board configuration
const char* localBoardId = "sender";     // This board
const char* remoteBoardId = "receiver";  // Remote board to control

// Define pins
const int statusLed = 2;    // LED to show test status
const int successLed = 19;  // LED to show successful sends
const int failLed = 21;     // LED to show failed sends

// Remote pin definitions
const int remoteLedPin = 13;  // Remote LED pin to control

// Test configuration
const int TOTAL_MESSAGES = 500;  // Total number of messages to send
const int SEND_DELAY_MS = 100;   // Delay between messages (ms)

// Test statistics
int messagesAttempted = 0;
int messagesSucceeded = 0;
int messagesFailedDespiteRetries = 0;
int messagesSucceededWithRetries = 0;
int firstAttemptSuccesses = 0;

// Message tracking
bool awaitingResponse = false;
unsigned long lastMessageTime = 0;
int currentSequence = 0;
bool currentValue = false;

// NetworkComm instance
NetworkComm netComm;

// Called for all message sends with the result (success or failure)
void onSendStatus(const char* targetBoardId, uint8_t messageType,
                  bool success) {
  // This callback is triggered on the initial send attempt (MAC layer)
  if (messageType == MSG_TYPE_PIN_CONTROL) {
    if (success) {
      // Count first-attempt successes
      firstAttemptSuccesses++;

      // Visual indicator
      digitalWrite(successLed, HIGH);
      delay(5);
      digitalWrite(successLed, LOW);
    } else {
      // Visual indicator for failure
      digitalWrite(failLed, HIGH);
      delay(5);
      digitalWrite(failLed, LOW);
    }
  }
}

// Called when a pin control operation completes (with or without retries)
void onPinControlComplete(const char* sender, uint8_t pin, uint8_t value,
                          bool success) {
  // This is called after all retry attempts (if any)
  awaitingResponse = false;

  if (success) {
    messagesSucceeded++;

    // If this succeeded but wasn't a first-attempt success, it succeeded with
    // retries
    if (messagesSucceeded > firstAttemptSuccesses) {
      messagesSucceededWithRetries = messagesSucceeded - firstAttemptSuccesses;
    }

    // Visual indicator
    digitalWrite(successLed, HIGH);
    delay(20);
    digitalWrite(successLed, LOW);
  } else {
    messagesFailedDespiteRetries++;

    // Visual indicator
    digitalWrite(failLed, HIGH);
    delay(50);
    digitalWrite(failLed, LOW);
  }

  // Print progress every 10 messages
  if (messagesAttempted % 10 == 0 || messagesAttempted == TOTAL_MESSAGES) {
    Serial.print("Progress: ");
    Serial.print(messagesAttempted);
    Serial.print("/");
    Serial.print(TOTAL_MESSAGES);
    Serial.print(" (");
    Serial.print((messagesAttempted * 100) / TOTAL_MESSAGES);
    Serial.println("%)");
  }

  // If we've completed all messages, show final results
  if (messagesAttempted == TOTAL_MESSAGES) {
    showResults();
  }
}

// Display final test results
void showResults() {
  Serial.println("\n========== RELIABILITY TEST RESULTS ==========");
  Serial.print("Total messages sent: ");
  Serial.println(TOTAL_MESSAGES);
  Serial.print("Messages succeeded on first attempt: ");
  Serial.print(firstAttemptSuccesses);
  Serial.print(" (");
  Serial.print((firstAttemptSuccesses * 100) / TOTAL_MESSAGES);
  Serial.println("%)");
  Serial.print("Messages succeeded with retries: ");
  Serial.print(messagesSucceededWithRetries);
  Serial.print(" (");
  Serial.print((messagesSucceededWithRetries * 100) / TOTAL_MESSAGES);
  Serial.println("%)");
  Serial.print("Messages failed despite retries: ");
  Serial.print(messagesFailedDespiteRetries);
  Serial.print(" (");
  Serial.print((messagesFailedDespiteRetries * 100) / TOTAL_MESSAGES);
  Serial.println("%)");
  Serial.print("Overall success rate: ");
  Serial.print((messagesSucceeded * 100) / TOTAL_MESSAGES);
  Serial.println("%");
  Serial.println("==============================================");

  // Blink status LED to indicate test completion
  for (int i = 0; i < 10; i++) {
    digitalWrite(statusLed, HIGH);
    delay(100);
    digitalWrite(statusLed, LOW);
    delay(100);
  }
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\nNetworkComm Pin Control Reliability Test");

  // Set up pins
  pinMode(statusLed, OUTPUT);
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

  // Enable debug logging (optional, comment out for less verbose output)
  // netComm.enableDebugLogging(true);

  // Register callbacks
  netComm.onSendStatus(onSendStatus);

  // Enable automatic retries for pin control messages
  netComm.enablePinControlRetries(true);

  // Configure retry parameters
  netComm.setPinControlMaxRetries(3);    // Maximum 3 retries
  netComm.setPinControlRetryDelay(500);  // 500ms between retries

  Serial.println("Automatic retries enabled with max 3 retries, 500ms delay");
  Serial.println("Starting reliability test with 500 messages...");

  // Indicate test is starting
  for (int i = 0; i < 3; i++) {
    digitalWrite(statusLed, HIGH);
    delay(200);
    digitalWrite(statusLed, LOW);
    delay(200);
  }

  // Initialize test
  lastMessageTime = millis();
}

void loop() {
  // Call NetworkComm update to process messages and retries
  netComm.update();

  // Check if we should send the next message
  unsigned long currentTime = millis();

  if (!awaitingResponse && messagesAttempted < TOTAL_MESSAGES &&
      (currentTime - lastMessageTime >= SEND_DELAY_MS)) {
    // Toggle the value for each message
    currentValue = !currentValue;
    currentSequence++;
    messagesAttempted++;

    // Send the message with the current sequence number
    Serial.print("Sending message #");
    Serial.print(currentSequence);
    Serial.print(", value: ");
    Serial.println(currentValue ? "HIGH" : "LOW");

    // Control the remote pin with confirmation callback
    awaitingResponse = true;
    netComm.controlRemotePin(remoteBoardId, remoteLedPin,
                             currentValue ? HIGH : LOW, onPinControlComplete);

    // Update the last message time
    lastMessageTime = currentTime;

    // Blink status LED to show activity
    digitalWrite(statusLed, HIGH);
    delay(5);
    digitalWrite(statusLed, LOW);
  }
}