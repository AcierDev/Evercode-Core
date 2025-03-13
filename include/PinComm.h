/**
 * PinComm.h - Library for UART-based communication between ESP32 boards
 * Updated from direct pin-based communication to use UART
 *
 * This library enables communication between ESP32 boards using
 * UART (Serial) connections for reliable data exchange.
 *
 * IMPORTANT USAGE NOTES:
 * - Requires 3 physical connections between boards (TX, RX, GND)
 * - Each board needs a unique ID
 * - The PinComm library handles communication via UART/Serial
 * - Supports pin control, messaging, discovery, and data exchange
 */

#ifndef PinComm_h
#define PinComm_h

#include <Arduino.h>
#include <ArduinoJson.h>

// Message types (same as NetworkComm for compatibility)
#define MSG_TYPE_PIN_CONTROL 1
#define MSG_TYPE_PIN_SUBSCRIBE 2
#define MSG_TYPE_PIN_PUBLISH 3
#define MSG_TYPE_MESSAGE 4
#define MSG_TYPE_SERIAL_DATA 5
#define MSG_TYPE_DIRECT_MESSAGE 6
#define MSG_TYPE_DISCOVERY 7
#define MSG_TYPE_DISCOVERY_RESPONSE 8
#define MSG_TYPE_ACKNOWLEDGEMENT 9
#define MSG_TYPE_PIN_READ_REQUEST 10
#define MSG_TYPE_PIN_READ_RESPONSE 11

// Maximum number of subscriptions
#define MAX_SUBSCRIPTIONS 20
// Maximum number of peer boards
#define MAX_PEERS 20
// Maximum number of queued pin read responses
#define MAX_QUEUED_RESPONSES 10
// Maximum message data size
#define MAX_PIN_DATA_SIZE 250

// Timeouts
#define ACK_TIMEOUT 5000                  // 5 seconds
#define PIN_CONTROL_CONFIRM_TIMEOUT 5000  // 5 seconds

// Retry settings
#define DEFAULT_MAX_RETRIES 3    // Default maximum number of retries
#define DEFAULT_RETRY_DELAY 500  // Default delay between retries (ms)
#define MAX_RETRY_DELAY 10000    // Maximum allowed retry delay (ms)

// UART communication settings
#define DEFAULT_BAUD_RATE 9600  // Default baud rate for UART communication
#define MAX_MESSAGE_LENGTH 128  // Maximum message length
#define FRAME_START_BYTE 0x7E   // Start byte for message frame
#define FRAME_END_BYTE 0x7F     // End byte for message frame
#define ESCAPE_BYTE 0x7D        // Escape byte for special characters

// Callback function types (same as NetworkComm for compatibility)
typedef void (*MessageCallback)(const char* sender, const char* topic,
                                const char* message);
typedef void (*PinChangeCallback)(const char* sender, uint8_t pin,
                                  uint8_t value);
typedef void (*SerialDataCallback)(const char* sender, const char* data);
typedef void (*DiscoveryCallback)(const char* boardId);
typedef void (*PinControlConfirmCallback)(const char* sender, uint8_t pin,
                                          uint8_t value, bool success);
typedef void (*SendStatusCallback)(const char* targetBoardId,
                                   uint8_t messageType, bool success);
typedef void (*SendFailureCallback)(const char* targetBoardId,
                                    uint8_t messageType, uint8_t pin,
                                    uint8_t value);
typedef void (*PinReadResponseCallback)(const char* sender, uint8_t pin,
                                        uint8_t value, bool success);

class PinComm {
 public:
  /**
   * Constructor for PinComm
   *
   * Initializes internal variables but does not start communication.
   * Call begin() to start UART-based communication.
   */
  PinComm();

  // ==================== Initialization ====================
  /**
   * Initialize the UART-based communication
   *
   * @param serialPort Pointer to the Stream object (Serial, Serial1,
   * SoftwareSerial, etc.)
   * @param boardId Unique identifier for this board (must be unique among
   * connected boards)
   * @param baudRate Communication speed (default: 9600) - only used if NULL is
   * passed for serialPort
   * @return true if initialization was successful, false otherwise
   */
  bool begin(Stream* serialPort, const char* boardId,
             uint32_t baudRate = DEFAULT_BAUD_RATE);

  /**
   * Main loop function that must be called regularly
   *
   * This function handles message processing, timeouts, acknowledgements, and
   * periodic tasks. It should be called in the Arduino loop().
   */
  void update();

  // ==================== Board Discovery & Status ====================
  /**
   * Check if the board is connected and communication is operational
   *
   * @return true if communication channels are initialized and working
   */
  bool isConnected();

  /**
   * Check if a specific board is available
   *
   * @param boardId The ID of the board to check
   * @return true if the board has been discovered, false otherwise
   */
  bool isBoardAvailable(const char* boardId);

  /**
   * Get the number of available boards
   *
   * @return The number of discovered peer boards
   */
  int getAvailableBoardsCount();

  /**
   * Get the name of an available board by index
   *
   * @param index The index of the board (0 to getAvailableBoardsCount()-1)
   * @return The board ID as a String, or empty string if index is out of range
   */
  String getAvailableBoardName(int index);

  /**
   * Set a callback for when a new board is discovered
   *
   * @param callback Function to call when a new board is discovered
   * @return true if the callback was set successfully
   */
  bool onBoardDiscovered(DiscoveryCallback callback);

  // ==================== Debug & Diagnostic Features ====================
  /**
   * Enable or disable message acknowledgements
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
   * Enable or disable debug logging
   *
   * @param enable true to enable debug logging, false to disable
   * @return true if the setting was applied successfully
   */
  bool enableDebugLogging(bool enable);

  /**
   * Check if debug logging is enabled
   *
   * @return true if debug logging is enabled, false otherwise
   */
  bool isDebugLoggingEnabled();

  /**
   * Enable or disable verbose logging
   *
   * @param enable true to enable verbose logging, false to disable
   * @return true if the setting was applied successfully
   */
  bool enableVerboseLogging(bool enable);

  /**
   * Check if verbose logging is enabled
   *
   * @return true if verbose logging is enabled, false otherwise
   */
  bool isVerboseLoggingEnabled();

  /**
   * Register a callback for message send status
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
   * Enable or disable automatic retries for pin control messages
   *
   * @param enable true to enable automatic retries, false to disable
   * @return true if the setting was applied successfully
   */
  bool enablePinControlRetries(bool enable);

  /**
   * Check if automatic retries are enabled
   *
   * @return true if automatic retries are enabled, false otherwise
   */
  bool isPinControlRetriesEnabled();

  /**
   * Configure the maximum number of retries for pin control messages
   *
   * @param maxRetries The maximum number of retries (0-10)
   * @return true if the setting was applied successfully
   */
  bool setPinControlMaxRetries(uint8_t maxRetries);

  /**
   * Get the current maximum number of retries for pin control messages
   *
   * @return The maximum number of retries
   */
  uint8_t getPinControlMaxRetries();

  /**
   * Configure the delay between retries for pin control messages
   *
   * @param retryDelayMs The delay between retries in milliseconds (50-10000)
   * @return true if the setting was applied successfully
   */
  bool setPinControlRetryDelay(uint16_t retryDelayMs);

  /**
   * Get the current delay between retries for pin control messages
   *
   * @return The delay between retries in milliseconds
   */
  uint16_t getPinControlRetryDelay();

  // ==================== Remote Pin Control (Controller Side)
  // ====================
  /**
   * Control a pin on a remote board
   *
   * @param targetBoardId The ID of the target board
   * @param pin The pin number to control
   * @param value The value to set (HIGH/LOW)
   * @param callback Optional callback to be called when the operation completes
   * @return true if the message was sent successfully
   */
  bool controlRemotePin(const char* targetBoardId, uint8_t pin, uint8_t value,
                        PinControlConfirmCallback callback = NULL);

  /**
   * Clear all pin control confirmation callbacks
   *
   * @return true if callbacks were cleared successfully
   */
  bool clearRemotePinConfirmCallback();

  /**
   * Read the value of a pin on a remote board
   *
   * @param targetBoardId The ID of the target board
   * @param pin The pin number to read
   * @param callback Function to call when the pin value is received or times
   * out
   * @return true if the request was sent successfully, false otherwise
   */
  bool readRemotePin(const char* targetBoardId, uint8_t pin,
                     PinReadResponseCallback callback);

  /**
   * Read the value of a pin on a remote board synchronously
   *
   * This method blocks until the response is received or times out
   *
   * @param targetBoardId The ID of the target board
   * @param pin The pin number to read
   * @return The pin value, or 0 if the request failed
   */
  uint8_t readRemotePinSync(const char* targetBoardId, uint8_t pin);

  // ==================== Remote Pin Control (Responder Side)
  // ====================
  /**
   * Set up handling of pin control messages
   *
   * @param callback Optional callback to process pin control requests. If NULL
   * (default), the library will automatically set pins directly
   * (pinMode+digitalWrite). If provided, your callback is responsible for
   * handling the pin.
   * @return true if successful
   */
  bool handlePinControl(PinChangeCallback callback = NULL);

  /**
   * Stop handling pin control messages
   *
   * @return true if successful
   */
  bool stopHandlingPinControl();

  /**
   * Accept pin control from a specific board for a specific pin
   *
   * @param controllerBoardId The ID of the board to accept control from
   * @param pin The pin to allow control of
   * @param callback Function to call when pin control is received
   * @return true if the subscription was added successfully
   */
  bool acceptPinControlFrom(const char* controllerBoardId, uint8_t pin,
                            PinChangeCallback callback);

  /**
   * Stop accepting pin control from a specific board for a specific pin
   *
   * @param controllerBoardId The ID of the board to stop accepting control from
   * @param pin The pin to stop allowing control of
   * @return true if the subscription was removed successfully
   */
  bool stopAcceptingPinControlFrom(const char* controllerBoardId, uint8_t pin);

  /**
   * Handle pin read requests from other boards
   *
   * @param callback Optional callback to handle pin read requests. If NULL
   * (default), the library will automatically read pins using digitalRead(). If
   * provided, your callback must return the pin value to send back.
   * @return true if successful
   */
  bool handlePinReadRequests(uint8_t (*pinReadCallback)(uint8_t pin) = NULL);

  /**
   * Stop handling pin read requests
   *
   * @return true if successful
   */
  bool stopHandlingPinReadRequests();

  // ==================== Pin State Broadcasting ====================
  /**
   * Broadcast the state of a pin to all boards
   *
   * @param pin The pin number to broadcast
   * @param value The pin value to broadcast
   * @return true if the broadcast was sent successfully
   */
  bool broadcastPinState(uint8_t pin, uint8_t value);

  /**
   * Listen for pin state broadcasts from a specific board for a specific pin
   *
   * @param broadcasterBoardId The ID of the board to listen to
   * @param pin The pin to listen for
   * @param callback Function to call when a pin state broadcast is received
   * @return true if the subscription was added successfully
   */
  bool listenForPinStateFrom(const char* broadcasterBoardId, uint8_t pin,
                             PinChangeCallback callback);

  /**
   * Stop listening for pin state broadcasts from a specific board for a
   * specific pin
   *
   * @param broadcasterBoardId The ID of the board to stop listening to
   * @param pin The pin to stop listening for
   * @return true if the subscription was removed successfully
   */
  bool stopListeningForPinStateFrom(const char* broadcasterBoardId,
                                    uint8_t pin);

  // ==================== Topic-based Messaging ====================
  /**
   * Publish a message to a topic that all boards can subscribe to
   *
   * @param topic The topic to publish to
   * @param message The message to publish
   * @return true if the message was sent successfully
   */
  bool publishTopic(const char* topic, const char* message);

  /**
   * Subscribe to a topic to receive messages
   *
   * @param topic The topic to subscribe to
   * @param callback Function to call when a message is received on this topic
   * @return true if the subscription was added successfully
   */
  bool subscribeTopic(const char* topic, MessageCallback callback);

  /**
   * Unsubscribe from a topic
   *
   * @param topic The topic to unsubscribe from
   * @return true if the subscription was removed successfully
   */
  bool unsubscribeTopic(const char* topic);

  // ==================== Serial Data Forwarding ====================
  /**
   * Forward serial data to all boards
   *
   * @param data The data to forward
   * @return true if the data was sent successfully
   */
  bool forwardSerialData(const char* data);

  /**
   * Receive serial data from other boards
   *
   * @param callback Function to call when serial data is received
   * @return true if the callback was set successfully
   */
  bool receiveSerialData(SerialDataCallback callback);

  /**
   * Stop receiving serial data
   *
   * @return true if the callback was cleared successfully
   */
  bool stopReceivingSerialData();

  // ==================== Direct Messaging ====================
  /**
   * Send a direct message to a specific board
   *
   * @param targetBoardId The ID of the board to send the message to
   * @param message The message to send
   * @return true if the message was sent successfully
   */
  bool sendMessageToBoardId(const char* targetBoardId, const char* message);

  /**
   * Receive direct messages from other boards
   *
   * @param callback Function to call when a direct message is received
   * @return true if the callback was set successfully
   */
  bool receiveMessagesFromBoards(MessageCallback callback);

 private:
  // Board identification
  char _boardId[32];
  bool _isConnected;
  bool _acknowledgementsEnabled;
  bool _debugLoggingEnabled;
  bool _verboseLoggingEnabled;
  bool _pinControlRetriesEnabled;
  uint8_t _pinControlMaxRetries;
  uint16_t _pinControlRetryDelay;

  // Communication configuration
  Stream* _serialPort;
  uint32_t _baudRate;

  // Buffer for receiving data
  uint8_t _receiveBuffer[MAX_PIN_DATA_SIZE];
  uint16_t _receiveBufferIndex;
  bool _isReceiving;
  bool _isEscaped;

  // Message tracking for acknowledgements
  static const int MAX_TRACKED_MESSAGES = 10;
  struct MessageTrack {
    char messageId[37];  // UUID string length
    char targetBoard[32];
    bool acknowledged;
    uint32_t sentTime;
    bool active;
    uint8_t messageType;

    // For pin control confirmations, store the callback for this specific
    // message
    PinControlConfirmCallback confirmCallback;

    // Store pin control data for callbacks
    uint8_t pin;
    uint8_t value;

    // Retry-related fields
    uint8_t retryCount;
    uint32_t nextRetryTime;
    bool retryScheduled;
  };

  MessageTrack _trackedMessages[MAX_TRACKED_MESSAGES];
  int _trackedMessageCount;

  // Status callbacks
  SendStatusCallback _sendStatusCallback;
  SendFailureCallback _sendFailureCallback;

  // Peer management
  struct PeerInfo {
    char boardId[32];
    bool active;
    uint32_t lastSeen;
  };

  PeerInfo _peers[MAX_PEERS];
  int _peerCount;

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
  uint8_t (*_pinReadCallback)(
      uint8_t pin);  // Callback for handling pin read requests

  // Message ID generation
  void generateMessageId(char* buffer);

  // UART-based communication methods
  bool sendFrame(const uint8_t* data, size_t length);
  bool receiveFrame();
  void processIncomingMessage(const uint8_t* data, size_t length);

  // Message sending helpers
  bool sendMessage(const char* targetBoard, uint8_t messageType,
                   const JsonObject& doc);
  bool broadcastMessage(uint8_t messageType, const JsonObject& doc);
  void sendAcknowledgement(const char* sender, const char* messageId);
  void handleAcknowledgement(const char* sender, const char* messageId);
  void handleDiscovery(const char* senderId);

  // Board discovery
  void broadcastPresence();
  bool addPeer(const char* boardId);
  uint32_t _lastDiscoveryBroadcast;

  // Pin read response handling
  struct PinReadResponse {
    char targetBoard[32];
    uint8_t pin;
    uint8_t value;
    bool success;
    char messageId[37];
    bool active;
    uint32_t queuedTime;
  };

  // Queue for pin read responses
  PinReadResponse _queuedResponses[MAX_QUEUED_RESPONSES];
  uint8_t _queuedResponseCount;

  // Method to queue a pin read response
  void queuePinReadResponse(const char* targetBoard, uint8_t pin, uint8_t value,
                            bool success, const char* messageId);

  // Method to process the queued responses
  void processQueuedResponses();
};

#endif