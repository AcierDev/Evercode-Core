/**
 * UARTExample.ino - Example sketch for PinComm library using UART communication
 *
 * This example demonstrates how to use the PinComm library to communicate
 * between two ESP32 boards using UART (Serial) connections.
 *
 * Hardware setup:
 * - Connect GND of both boards together
 * - Connect TX of Board A to RX of Board B
 * - Connect RX of Board A to TX of Board B
 *
 * Instructions:
 * 1. Upload this sketch to both ESP32 boards
 * 2. For one board, set IS_BOARD_A to true
 * 3. For the other board, set IS_BOARD_A to false
 * 4. Open Serial Monitor for both boards to see the communication
 *
 * This example demonstrates:
 * - Board discovery
 * - Direct messaging
 * - Remote pin control
 * - Topic-based messaging
 */

#include <PinComm.h>

// Set this to true for Board A, false for Board B
#define IS_BOARD_A true

// Define board IDs
#define BOARD_A_ID "ESP32-A"
#define BOARD_B_ID "ESP32-B"

// Define pins for UART communication
// Note: ESP32 has multiple hardware serial ports
// We'll use Serial2 for PinComm communication
#define RX_PIN 16  // GPIO16 for RX
#define TX_PIN 17  // GPIO17 for TX

// Define a pin to control remotely
#define LED_PIN 2  // Built-in LED on most ESP32 boards

// Create PinComm instance
PinComm pinComm;

// For Board B, we'll use this to track if the LED state has changed
bool lastLedState = false;

// Callback for when a new board is discovered
void onBoardDiscovered(const char* boardId) {
  Serial.print("New board discovered: ");
  Serial.println(boardId);
}

// Callback for when a direct message is received
void onMessageReceived(const char* sender, const char* topic,
                       const char* message) {
  Serial.print("Message from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(message);
}

// Callback for when a pin control message is received
void onPinControlReceived(const char* sender, uint8_t pin, uint8_t value) {
  Serial.print("Pin control from ");
  Serial.print(sender);
  Serial.print(": Pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.println(value);

  // Set the pin to the requested value
  digitalWrite(pin, value);
}

// Callback for when a pin control operation is confirmed
void onPinControlConfirmed(const char* sender, uint8_t pin, uint8_t value,
                           bool success) {
  if (success) {
    Serial.print("Successfully set pin ");
    Serial.print(pin);
    Serial.print(" to ");
    Serial.print(value);
    Serial.print(" on board ");
    Serial.println(sender);
  } else {
    Serial.print("Failed to set pin ");
    Serial.print(pin);
    Serial.print(" on board ");
    Serial.println(sender);
  }
}

// Callback for topic-based messages
void onTopicMessage(const char* sender, const char* topic,
                    const char* message) {
  Serial.print("Topic message from ");
  Serial.print(sender);
  Serial.print(" on topic '");
  Serial.print(topic);
  Serial.print("': ");
  Serial.println(message);
}

void setup() {
  // Initialize Serial for debugging
  Serial.begin(115200);
  Serial.println("\n\nPinComm UART Example");

  // Initialize the LED pin
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize Serial2 for PinComm communication
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);

  // Set the board ID based on the IS_BOARD_A flag
  const char* boardId = IS_BOARD_A ? BOARD_A_ID : BOARD_B_ID;
  Serial.print("Initializing as board: ");
  Serial.println(boardId);

  // Initialize PinComm with Serial2
  if (pinComm.begin(&Serial2, boardId)) {
    Serial.println("PinComm initialized successfully");
  } else {
    Serial.println("Failed to initialize PinComm");
    while (1) delay(100);  // Stop if initialization failed
  }

  // Enable debug logging
  pinComm.enableDebugLogging(true);

  // Set up callbacks
  pinComm.onBoardDiscovered(onBoardDiscovered);
  pinComm.receiveMessagesFromBoards(onMessageReceived);
  pinComm.handlePinControl(onPinControlReceived);
  pinComm.subscribeTopic("status", onTopicMessage);

  // Board A will control Board B's LED
  if (IS_BOARD_A) {
    Serial.println("Board A: Will control Board B's LED");
  }
  // Board B will accept pin control from Board A
  else {
    Serial.println("Board B: Will accept pin control from Board A");
    pinComm.acceptPinControlFrom(BOARD_A_ID, LED_PIN, onPinControlReceived);
  }

  Serial.println("Setup complete. Waiting for communication...");
}

void loop() {
  // Update PinComm (must be called regularly)
  pinComm.update();

  // Board A specific behavior
  if (IS_BOARD_A) {
    static uint32_t lastMessageTime = 0;
    static uint32_t lastLedToggleTime = 0;
    static bool ledState = false;

    // Send a direct message to Board B every 5 seconds
    if (millis() - lastMessageTime > 5000) {
      if (pinComm.isBoardAvailable(BOARD_B_ID)) {
        char message[64];
        sprintf(message, "Hello from Board A! Uptime: %lu ms", millis());

        Serial.print("Sending message to Board B: ");
        Serial.println(message);

        pinComm.sendMessageToBoardId(BOARD_B_ID, message);

        // Also publish to a topic
        pinComm.publishTopic("status", "Board A is running normally");
      } else {
        Serial.println("Board B not available yet");
      }

      lastMessageTime = millis();
    }

    // Toggle Board B's LED every 2 seconds
    if (millis() - lastLedToggleTime > 2000) {
      if (pinComm.isBoardAvailable(BOARD_B_ID)) {
        ledState = !ledState;

        Serial.print("Setting Board B's LED to: ");
        Serial.println(ledState ? "ON" : "OFF");

        pinComm.controlRemotePin(BOARD_B_ID, LED_PIN, ledState,
                                 onPinControlConfirmed);
      }

      lastLedToggleTime = millis();
    }
  }
  // Board B specific behavior
  else {
    static uint32_t lastStatusTime = 0;

    // Board B will send its LED status every 3 seconds
    if (millis() - lastStatusTime > 3000) {
      bool currentLedState = digitalRead(LED_PIN);

      // Only send if the state has changed or it's time for a periodic update
      if (currentLedState != lastLedState ||
          millis() - lastStatusTime > 10000) {
        char message[64];
        sprintf(message, "LED is %s", currentLedState ? "ON" : "OFF");

        Serial.print("Publishing LED status: ");
        Serial.println(message);

        pinComm.publishTopic("status", message);
        lastLedState = currentLedState;
      }

      lastStatusTime = millis();
    }
  }

  // Small delay to avoid hogging the CPU
  delay(10);
}