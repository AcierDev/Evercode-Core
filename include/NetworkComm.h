/**
 * NetworkComm.h - Library for communication between ESP32 boards
 * Created by Claude, 2023
 *
 * This library enables communication between ESP32 boards on the
 * same network using mDNS for discovery and MQTT-like pub/sub patterns for data
 * exchange.
 */

#ifndef NetworkComm_h
#define NetworkComm_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <PubSubClient.h>
#include <WiFi.h>

// Message types
#define MSG_TYPE_PIN_CONTROL 1
#define MSG_TYPE_PIN_SUBSCRIBE 2
#define MSG_TYPE_PIN_PUBLISH 3
#define MSG_TYPE_MESSAGE 4
#define MSG_TYPE_SERIAL_DATA 5
#define MSG_TYPE_DIRECT_MESSAGE 6

// Maximum number of subscriptions
#define MAX_SUBSCRIPTIONS 20

// Callback function types
typedef void (*MessageCallback)(const char* sender, const char* topic,
                                const char* message);
typedef void (*PinChangeCallback)(const char* sender, uint8_t pin,
                                  uint8_t value);
typedef void (*SerialDataCallback)(const char* sender, const char* data);

class NetworkComm {
 public:
  // Constructor
  NetworkComm();

  // Initialization
  bool begin(const char* ssid, const char* password, const char* boardId);

  // Main loop function - must be called in loop()
  void update();

  // Board discovery
  bool isConnected();
  bool isBoardAvailable(const char* boardId);
  int getAvailableBoardsCount();
  String getAvailableBoardName(int index);

  // Pin control
  bool setPinValue(const char* targetBoard, uint8_t pin, uint8_t value);
  uint8_t getPinValue(const char* targetBoard, uint8_t pin);

  // Pin subscription
  bool subscribeToPinChange(const char* targetBoard, uint8_t pin,
                            PinChangeCallback callback);
  bool unsubscribeFromPinChange(const char* targetBoard, uint8_t pin);

  // Message pub/sub
  bool publish(const char* topic, const char* message);
  bool subscribe(const char* topic, MessageCallback callback);
  bool unsubscribe(const char* topic);

  // Serial data pub/sub
  bool publishSerialData(const char* data);
  bool subscribeToSerialData(SerialDataCallback callback);
  bool unsubscribeFromSerialData();

  // Direct messaging
  bool sendDirectMessage(const char* targetBoard, const char* message);
  bool setDirectMessageCallback(MessageCallback callback);

 private:
  // Network configuration
  char _boardId[32];
  bool _isConnected;

  // mDNS handling
  void setupMDNS();
  void checkForNewBoards();

  // Message handling
  void processIncomingMessage(const char* topic, byte* payload,
                              unsigned int length);
  void sendMessage(const char* targetBoard, uint8_t messageType,
                   const JsonDocument& doc);

  // Subscription management
  struct Subscription {
    char topic[32];
    char targetBoard[32];
    uint8_t pin;
    uint8_t type;
    void* callback;
    bool active;
  };

  Subscription _subscriptions[MAX_SUBSCRIPTIONS];
  int _subscriptionCount;

  // Callback handlers
  MessageCallback _directMessageCallback;
  SerialDataCallback _serialDataCallback;

  // ESP32 WiFi client
  WiFiClient _wifiClient;
  PubSubClient _mqttClient;
};

#endif