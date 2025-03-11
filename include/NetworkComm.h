/**
 * NetworkComm.h - Library for communication between ESP32 boards
 * Created by Claude, 2023
 * Modified to use ESP-NOW for direct communication
 *
 * This library enables communication between ESP32 boards using
 * ESP-NOW for direct peer-to-peer data exchange.
 *
 * IMPORTANT USAGE NOTES:
 * - When using pin control functionality, you must still configure pin modes on
 * the target board
 * - Example: For remote LED control, use pinMode(LED_PIN, OUTPUT) on the board
 * with the LED
 * - The NetworkComm library handles communication but not pin configuration
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
#define MSG_TYPE_PIN_CONTROL_CONFIRM 10
#define MSG_TYPE_PIN_CONTROL_RESPONSE 11

// Maximum number of subscriptions
#define MAX_SUBSCRIPTIONS 20
// Maximum number of peer boards
#define MAX_PEERS 20
// Maximum ESP-NOW data size
#define MAX_ESP_NOW_DATA_SIZE 250

// Timeouts
#define ACK_TIMEOUT 5000                  // 5 seconds
#define PIN_CONTROL_CONFIRM_TIMEOUT 5000  // 5 seconds

// Callback function types
typedef void (*MessageCallback)(const char* sender, const char* topic,
                                const char* message);
typedef void (*PinChangeCallback)(const char* sender, uint8_t pin,
                                  uint8_t value);
typedef void (*SerialDataCallback)(const char* sender, const char* data);
typedef void (*DiscoveryCallback)(const char* boardId);
typedef void (*PinControlConfirmCallback)(const char* sender, uint8_t pin,
                                          uint8_t value, bool success);

class NetworkComm {
 public:
  // Constructor
  NetworkComm();

  // ==================== Initialization ====================
  bool begin(const char* ssid, const char* password, const char* boardId);

  // Main loop function - must be called in loop()
  void update();

  // ==================== Board Discovery & Network Status ====================
  bool isConnected();
  bool isBoardAvailable(const char* boardId);
  int getAvailableBoardsCount();
  String getAvailableBoardName(int index);
  bool onBoardDiscovered(DiscoveryCallback callback);

  // ==================== Debug & Diagnostic Features ====================
  bool enableMessageAcknowledgements(bool enable);
  bool isAcknowledgementsEnabled();
  bool enableDebugLogging(bool enable);
  bool isDebugLoggingEnabled();

  // ==================== Remote Pin Control (Controller Side)
  // ====================
  bool controlRemotePin(const char* targetBoardId, uint8_t pin, uint8_t value);
  bool controlRemotePinWithConfirmation(const char* targetBoardId, uint8_t pin,
                                        uint8_t value,
                                        PinControlConfirmCallback callback);
  bool clearRemotePinConfirmCallback();
  uint8_t readRemotePin(const char* targetBoardId, uint8_t pin);

  // ==================== Remote Pin Control (Responder Side)
  // ====================
  bool acceptPinControlFrom(const char* controllerBoardId, uint8_t pin,
                            PinChangeCallback callback);
  bool stopAcceptingPinControlFrom(const char* controllerBoardId, uint8_t pin);

  // ==================== Pin State Broadcasting ====================
  bool broadcastPinState(uint8_t pin, uint8_t value);
  bool listenForPinStateFrom(const char* broadcasterBoardId, uint8_t pin,
                             PinChangeCallback callback);
  bool stopListeningForPinStateFrom(const char* broadcasterBoardId,
                                    uint8_t pin);

  // ==================== Topic-based Messaging ====================
  bool publishTopic(const char* topic, const char* message);
  bool subscribeTopic(const char* topic, MessageCallback callback);
  bool unsubscribeTopic(const char* topic);

  // ==================== Serial Data Forwarding ====================
  bool forwardSerialData(const char* data);
  bool receiveSerialData(SerialDataCallback callback);
  bool stopReceivingSerialData();

  // ==================== Direct Messaging ====================
  bool sendMessageToBoardId(const char* targetBoardId, const char* message);
  bool receiveMessagesFromBoards(MessageCallback callback);

  // ==================== Deprecated API (for backward compatibility)
  // ==================== These methods are kept for backward compatibility but
  // will be removed in future versions

  // Pin control (deprecated)
  bool setPinValue(const char* targetBoard, uint8_t pin, uint8_t value) {
    return controlRemotePin(targetBoard, pin, value);
  }

  bool setPinValueWithConfirmation(const char* targetBoard, uint8_t pin,
                                   uint8_t value,
                                   PinControlConfirmCallback callback) {
    return controlRemotePinWithConfirmation(targetBoard, pin, value, callback);
  }

  bool clearPinControlConfirmCallback() {
    return clearRemotePinConfirmCallback();
  }

  uint8_t getPinValue(const char* targetBoard, uint8_t pin) {
    return readRemotePin(targetBoard, pin);
  }

  // Pin subscription (deprecated)
  bool subscribeToPinChange(const char* targetBoard, uint8_t pin,
                            PinChangeCallback callback) {
    return acceptPinControlFrom(targetBoard, pin, callback);
  }

  bool unsubscribeFromPinChange(const char* targetBoard, uint8_t pin) {
    return stopAcceptingPinControlFrom(targetBoard, pin);
  }

  // Message pub/sub (deprecated)
  bool publish(const char* topic, const char* message) {
    return publishTopic(topic, message);
  }

  bool subscribe(const char* topic, MessageCallback callback) {
    return subscribeTopic(topic, callback);
  }

  bool unsubscribe(const char* topic) { return unsubscribeTopic(topic); }

  // Serial data pub/sub (deprecated)
  bool publishSerialData(const char* data) { return forwardSerialData(data); }

  bool subscribeToSerialData(SerialDataCallback callback) {
    return receiveSerialData(callback);
  }

  bool unsubscribeFromSerialData() { return stopReceivingSerialData(); }

  // Direct messaging (deprecated)
  bool sendDirectMessage(const char* targetBoard, const char* message) {
    return sendMessageToBoardId(targetBoard, message);
  }

  bool setDirectMessageCallback(MessageCallback callback) {
    return receiveMessagesFromBoards(callback);
  }

  // Board Discovery (deprecated)
  bool setDiscoveryCallback(DiscoveryCallback callback) {
    return onBoardDiscovered(callback);
  }

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
    uint8_t messageType;  // Store the message type
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
  PinControlConfirmCallback _pinControlConfirmCallback;

  // Internal helper functions
  uint32_t _lastDiscoveryBroadcast;
  void performDiscovery();
};

#endif