# NetworkComm Library

A lightweight library for communication between ESP32 boards on the same network.

## Features

- **mDNS Discovery**: Automatically discover other boards on the network
- **Remote I/O Control**: Control pins on remote boards
- **Publisher-Subscriber Pattern**: For I/O pins, messages, and serial data
- **Direct Messaging**: Send messages directly to specific boards

## Requirements

- WiFi
- ESPmDNS
- PubSubClient
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

### Remote Pin Control

```cpp
// Set a pin on a remote board
netComm.setPinValue("board2", 13, HIGH);

// Get a pin value from a remote board
uint8_t value = netComm.getPinValue("board2", 13);
```

### Pin Subscription

```cpp
// Callback function
void onPinChanged(const char* sender, uint8_t pin, uint8_t value) {
  Serial.print("Pin change from ");
  Serial.print(sender);
  Serial.print(": Pin ");
  Serial.print(pin);
  Serial.print(" = ");
  Serial.println(value);
}

// Subscribe to pin changes
netComm.subscribeToPinChange("board2", 13, onPinChanged);

// Unsubscribe
netComm.unsubscribeFromPinChange("board2", 13);
```

### Message Pub/Sub

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
netComm.subscribe("test/topic", onMessageReceived);

// Publish to a topic
netComm.publish("test/topic", "Hello, world!");

// Unsubscribe
netComm.unsubscribe("test/topic");
```

### Serial Data Pub/Sub

```cpp
// Callback function
void onSerialData(const char* sender, const char* data) {
  Serial.print("Serial data from ");
  Serial.print(sender);
  Serial.print(": ");
  Serial.println(data);
}

// Subscribe to serial data
netComm.subscribeToSerialData(onSerialData);

// Publish serial data
netComm.publishSerialData("Hello from serial");

// Unsubscribe
netComm.unsubscribeFromSerialData();
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

// Set callback for direct messages
netComm.setDirectMessageCallback(onDirectMessage);

// Send direct message
netComm.sendDirectMessage("board2", "Hello, board2!");
```

## Board Discovery

```cpp
// Check if a board is available
bool available = netComm.isBoardAvailable("board2");

// Get count of available boards
int count = netComm.getAvailableBoardsCount();

// Get name of available board by index
String name = netComm.getAvailableBoardName(0);
```

## Notes

- This library requires a MQTT broker on the network. The broker can be hosted on one of the boards or on a separate device.
- The library uses mDNS to discover the MQTT broker and other boards on the network.
- All communication is done using JSON messages for compatibility and flexibility.

## License

This library is released under the MIT License.
