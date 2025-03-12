# NetworkComm Library

A lightweight library for direct communication between ESP32 boards using ESP-NOW.

## Features

- **ESP-NOW Communication**: Direct peer-to-peer communication without requiring a broker
- **Automatic Board Discovery**: Boards automatically detect each other on the network
- **Remote Pin Control**: Control pins on remote boards with optional confirmation
- **Unified Pin Control API**: Simple API for controlling remote pins with or without callbacks
- **Publisher-Subscriber Pattern**: For I/O pins, messages, and serial data
- **Direct Messaging**: Send messages directly to specific boards

## Requirements

- ESP32 boards
- WiFi
- ESP-NOW (included in ESP32 Arduino core)
- ArduinoJson

## Installation

1. Clone this repository or download the ZIP file
2. Extract the contents to your Arduino/libraries folder or PlatformIO project
3. Include the library in your sketch: `#include "NetworkComm.h"`

## Usage

### Initialization

```cpp
// Configure NetworkComm
NetworkComm netComm;
netComm.begin("YourWiFiSSID", "YourWiFiPassword", "board1");
```

### Main Loop

Make sure to call `update()` in your main loop:

```cpp
void loop() {
  netComm.update();
  // Your code here
}
```

### Remote Pin Control (Controller Side)

```cpp
// Simple pin control (no callback, no confirmation)
netComm.controlRemotePin("board2", 13, HIGH);

// Pin control with callback but no confirmation
netComm.controlRemotePin("board2", 13, HIGH, myCallback);

// Pin control with callback and confirmation
netComm.controlRemotePin("board2", 13, HIGH, myCallback, true);

// Callback function for pin control
void myCallback(const char* sender, uint8_t pin, uint8_t value, bool success) {
  Serial.print("Pin control on ");
  Serial.print(sender);
  Serial.print(": Pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.print(value);
  Serial.print(", Success: ");
  Serial.println(success ? "Yes" : "No");
}
```

### Remote Pin Control (Responder Side)

```cpp
// Automatic pin handling (easiest)
// Library will automatically set pinMode/digitalWrite for you
netComm.handlePinControl();

// Custom pin handling (more flexible)
netComm.handlePinControl(myPinChangeHandler);

// Handler function for pin changes
void myPinChangeHandler(const char* sender, uint8_t pin, uint8_t value) {
  Serial.print("Pin change request from ");
  Serial.print(sender);
  Serial.print(": Pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.println(value);

  // Custom handling of the pin
  if (pin == 13) {
    digitalWrite(pin, value);
    // Additional custom actions
    playTone(value ? 1000 : 500, 100);
  }
}

// Stop handling pin control
netComm.stopHandlingPinControl();
```

### Pin State Broadcasting

```cpp
// Broadcast the state of a pin to all boards
netComm.broadcastPinState(13, HIGH);

// Listen for pin state broadcasts
netComm.listenForPinStateFrom("board2", 13, myPinChangeHandler);

// Stop listening
netComm.stopListeningForPinStateFrom("board2", 13);
```

### Topic-based Messaging

```cpp
// Callback function
void onMessageReceived(const char* sender, const char* topic, const char* message) {
  Serial.print("Message from ");
  Serial.print(sender);
  Serial.print(" on topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);
}

// Subscribe to a topic
netComm.subscribeTopic("test/topic", onMessageReceived);

// Publish to a topic
netComm.publishTopic("test/topic", "Hello, world!");

// Unsubscribe
netComm.unsubscribeTopic("test/topic");
```

### Serial Data Forwarding

```cpp
// Callback function
void onSerialData(const char* sender, const char* data) {
  Serial.print("Serial data from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(data);
}

// Receive serial data
netComm.receiveSerialData(onSerialData);

// Forward serial data
netComm.forwardSerialData("Hello from serial");

// Stop receiving
netComm.stopReceivingSerialData();
```

### Direct Messaging

```cpp
// Callback function
void onDirectMessage(const char* sender, const char* topic, const char* message) {
  Serial.print("Direct message from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(message);
}

// Receive messages
netComm.receiveMessagesFromBoards(onDirectMessage);

// Send direct message
netComm.sendMessageToBoardId("board2", "Hello, board2!");
```

## Board Discovery

```cpp
// Callback for when a new board is discovered
void onBoardDiscovered(const char* boardId) {
  Serial.print("Discovered board: ");
  Serial.println(boardId);
}

// Set discovery callback
netComm.onBoardDiscovered(onBoardDiscovered);

// Check if a board is available
bool available = netComm.isBoardAvailable("board2");

// Get count of available boards
int count = netComm.getAvailableBoardsCount();

// Get name of available board by index
String name = netComm.getAvailableBoardName(0);
```

## Debugging

```cpp
// Enable/disable message acknowledgements
netComm.enableMessageAcknowledgements(true);

// Enable/disable debug logging
netComm.enableDebugLogging(true);

// Enable/disable verbose logging
netComm.enableVerboseLogging(false);
```

## Notes

- This library uses ESP-NOW for direct peer-to-peer communication between ESP32 boards
- No need for a broker or central server
- All communication is done using JSON messages for compatibility and flexibility
- The library handles basic pin control automatically if no callback is provided

## License

This library is released under the MIT License.
