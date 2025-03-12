/**
 * NetworkCore.h - Base class for ESP32 network communication
 * Created as part of the NetworkComm library refactoring
 *
 * This class provides the core functionality for ESP-NOW based
 * communication between ESP32 boards.
 */

#ifndef NetworkCore_h
#define NetworkCore_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_now.h>

// Forward declaration
class NetworkDiscovery;

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

// Maximum number of peer boards
#define MAX_PEERS 20
// Maximum ESP-NOW data size
#define MAX_ESP_NOW_DATA_SIZE 250

// Timeouts
#define ACK_TIMEOUT 5000  // 5 seconds

// Callback function types for send status
typedef void (*SendStatusCallback)(const char* targetBoardId,
                                   uint8_t messageType, bool success);
typedef void (*SendFailureCallback)(const char* targetBoardId,
                                    uint8_t messageType, uint8_t pin,
                                    uint8_t value);

class NetworkCore {
 public:
  /**
   * Constructor for NetworkCore
   */
  NetworkCore();

  /**
   * Virtual destructor
   */
  virtual ~NetworkCore();

  // ==================== Initialization ====================
  /**
   * Initialize the network communication
   *
   * @param ssid WiFi network SSID to connect to
   * @param password WiFi network password
   * @param boardId Unique identifier for this board (must be unique on the
   * network)
   * @return true if initialization was successful, false otherwise
   */
  bool begin(const char* ssid, const char* password, const char* boardId);

  /**
   * Main loop function that must be called regularly
   *
   * This function handles message timeouts, acknowledgements, and periodic
   * tasks. It should be called in the Arduino loop().
   */
  void update();

  /**
   * Check if the board is connected to the network
   *
   * @return true if connected to WiFi and ESP-NOW is initialized
   */
  bool isConnected();

  // ==================== Message Handling ====================
  /**
   * Enable or disable message acknowledgements
   *
   * When enabled, message delivery will be confirmed with acknowledgements.
   *
   * @param enable true to enable acknowledgements, false to disable
   * @return true if the setting was applied successfully
   */
  bool enableMessageAcknowledgements(bool enable);

  /**
   * Check if message acknowledgements are enabled
   *
   * @return true if acknowledgements are enabled, false otherwise
   */
  bool isAcknowledgementsEnabled();

  /**
   * Register a callback for ESP-NOW send status
   *
   * @param callback Function to call when a message send status is received
   * @return true if the callback was set successfully
   */
  bool onSendStatus(SendStatusCallback callback);

  /**
   * Register a callback specifically for message delivery failures
   *
   * @param callback Function to call when a message fails to deliver
   * @return true if the callback was set successfully
   */
  bool onSendFailure(SendFailureCallback callback);

  /**
   * Register the Discovery instance to handle discovery messages
   *
   * @param discovery Pointer to the NetworkDiscovery instance
   * @return true if registered successfully
   */
  bool registerDiscoveryHandler(NetworkDiscovery* discovery);

 protected:
  // Board identification
  char _boardId[32];
  uint8_t _macAddress[6];
  bool _isConnected;
  bool _acknowledgementsEnabled;
  bool _debugLoggingEnabled;
  bool _verboseLoggingEnabled;

  // Message tracking for acknowledgements
  static const int MAX_TRACKED_MESSAGES = 10;
  struct MessageTrack {
    char messageId[37];  // UUID string length
    char targetBoard[32];
    bool acknowledged;
    uint32_t sentTime;
    bool active;
    uint8_t messageType;    // Store the message type
    void* confirmCallback;  // Generic pointer for callbacks
    uint8_t pin;            // For pin control
    uint8_t value;          // For pin control
  };

  MessageTrack _trackedMessages[MAX_TRACKED_MESSAGES];
  int _trackedMessageCount;

  // Peer management
  struct PeerInfo {
    char boardId[32];
    uint8_t macAddress[6];
    bool active;
    uint32_t lastSeen;
  };

  PeerInfo _peers[MAX_PEERS];
  int _peerCount;

  // ESP-NOW callbacks
  static void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status);
  static void onDataReceived(const uint8_t* mac, const uint8_t* data, int len);

  void handleSendStatus(const uint8_t* mac_addr, esp_now_send_status_t status);
  void processIncomingMessage(const uint8_t* mac, const uint8_t* data, int len);

  // Callbacks
  SendStatusCallback _sendStatusCallback;
  SendFailureCallback _sendFailureCallback;

  // Helper methods for message handling
  void generateMessageId(char* buffer);

  bool sendMessage(const char* targetBoard, uint8_t messageType,
                   const JsonObject& doc);
  bool broadcastMessage(uint8_t messageType, const JsonObject& doc);

  bool getMacForBoardId(const char* boardId, uint8_t* macAddress);
  bool getBoardIdForMac(const uint8_t* macAddress, char* boardId);

  // Message acknowledgement handling
  void sendAcknowledgement(const char* sender, const char* messageId);
  void handleAcknowledgement(const char* sender, const char* messageId);

  // Debug logging helpers
  void debugLog(const char* event, const char* details = nullptr);
  void verboseLog(const char* event, const char* details = nullptr);

  // Peer management
  bool addPeer(const char* boardId, const uint8_t* macAddress);

  // Discovery handler
  NetworkDiscovery* _discoveryHandler;

  // Static instance pointer for callbacks
  static NetworkCore* _instance;

  // Friend classes that need access to protected members
  friend class NetworkDiscovery;
  friend class NetworkPinControl;
  friend class NetworkMessaging;
  friend class NetworkSerial;
  friend class NetworkDiagnostics;
};

#endif