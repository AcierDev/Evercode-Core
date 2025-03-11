/**
 * NetworkComm.h - Library for communication between ESP32 boards
 * Created by Claude, 2023
 * Modified to use ESP-NOW for direct communication
 *
 * This library enables communication between ESP32 boards using
 * ESP-NOW for direct peer-to-peer data exchange.
 */

#ifndef NetworkComm_h
#define NetworkComm_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_now.h>

// Message types
#define MSG_TYPE_PIN_CONTROL 1
#define MSG_TYPE_PIN_SUBSCRIBE 2
#define MSG_TYPE_PIN_PUBLISH 3
#define MSG_TYPE_MESSAGE 4
#define MSG_TYPE_SERIAL_DATA 5
#define MSG_TYPE_DIRECT_MESSAGE 6
#define MSG_TYPE_DISCOVERY 7
#define MSG_TYPE_DISCOVERY_RESPONSE 8
#define MSG_TYPE_ACKNOWLEDGEMENT 9

// Maximum number of subscriptions
#define MAX_SUBSCRIPTIONS 20
// Maximum number of peer boards
#define MAX_PEERS 20
// Maximum ESP-NOW data size
#define MAX_ESP_NOW_DATA_SIZE 250

// Callback function types
typedef void (*MessageCallback)(const char* sender, const char* topic,
                                const char* message);
typedef void (*PinChangeCallback)(const char* sender, uint8_t pin,
                                  uint8_t value);
typedef void (*SerialDataCallback)(const char* sender, const char* data);
typedef void (*DiscoveryCallback)(const char* boardId);

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

  // Debug features
  bool enableMessageAcknowledgements(bool enable);
  bool isAcknowledgementsEnabled();
  bool enableDebugLogging(bool enable);
  bool isDebugLoggingEnabled();

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

  // Board Discovery
  bool setDiscoveryCallback(DiscoveryCallback callback);

 private:
  // Board identification
  char _boardId[32];
  uint8_t _macAddress[6];
  bool _isConnected;
  bool _acknowledgementsEnabled;
  bool _debugLoggingEnabled;

  // Message tracking for acknowledgements
  static const int MAX_TRACKED_MESSAGES = 10;
  struct MessageTrack {
    char messageId[37];  // UUID string length
    char targetBoard[32];
    bool acknowledged;
    uint32_t sentTime;
    bool active;
  };

  MessageTrack _trackedMessages[MAX_TRACKED_MESSAGES];
  int _trackedMessageCount;

  // Message acknowledgement handling
  void sendAcknowledgement(const char* sender, const char* messageId);
  void handleAcknowledgement(const char* sender, const char* messageId);
  void generateMessageId(char* buffer);

  // Peer management
  struct PeerInfo {
    char boardId[32];
    uint8_t macAddress[6];
    bool active;
    uint32_t lastSeen;
  };

  PeerInfo _peers[MAX_PEERS];
  int _peerCount;

  // Discovery handling
  void broadcastPresence();
  void handleDiscovery(const char* senderId, const uint8_t* senderMac);
  bool addPeer(const char* boardId, const uint8_t* macAddress);
  bool getMacForBoardId(const char* boardId, uint8_t* macAddress);

  // Message handling
  static void onDataReceived(const uint8_t* mac, const uint8_t* data, int len);
  void processIncomingMessage(const uint8_t* mac, const uint8_t* data, int len);
  bool sendMessage(const char* targetBoard, uint8_t messageType,
                   const JsonObject& doc);
  bool broadcastMessage(uint8_t messageType, const JsonObject& doc);

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
  DiscoveryCallback _discoveryCallback;

  // Internal helper functions
  uint32_t _lastDiscoveryBroadcast;
  void performDiscovery();
};

#endif