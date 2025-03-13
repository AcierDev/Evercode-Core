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
#define MSG_TYPE_PIN_READ_REQUEST 10
#define MSG_TYPE_PIN_READ_RESPONSE 11

// Maximum number of subscriptions
#define MAX_SUBSCRIPTIONS 20
// Maximum number of peer boards
#define MAX_PEERS 20
// Maximum number of queued pin read responses
#define MAX_QUEUED_RESPONSES 10
// Maximum ESP-NOW data size
#define MAX_ESP_NOW_DATA_SIZE 250

// Timeouts
#define ACK_TIMEOUT 5000                  // 5 seconds
#define PIN_CONTROL_CONFIRM_TIMEOUT 5000  // 5 seconds

// Retry settings
#define DEFAULT_MAX_RETRIES 3    // Default maximum number of retries
#define DEFAULT_RETRY_DELAY 500  // Default delay between retries (ms)
#define MAX_RETRY_DELAY 10000    // Maximum allowed retry delay (ms)

// Callback function types
typedef void (*MessageCallback)(const char* sender, const char* topic,
                                const char* message);
typedef void (*PinChangeCallback)(const char* sender, uint8_t pin,
                                  uint8_t value);
typedef void (*SerialDataCallback)(const char* sender, const char* data);
typedef void (*DiscoveryCallback)(const char* boardId);
typedef void (*PinControlConfirmCallback)(const char* sender, uint8_t pin,
                                          uint8_t value, bool success);
// New callback type for ESP-NOW send status
typedef void (*SendStatusCallback)(const char* targetBoardId,
                                   uint8_t messageType, bool success);
// New callback specifically for send failures
typedef void (*SendFailureCallback)(const char* targetBoardId,
                                    uint8_t messageType, uint8_t pin,
                                    uint8_t value);
// New callback type for pin read responses
typedef void (*PinReadResponseCallback)(const char* sender, uint8_t pin,
                                        uint8_t value, bool success);

class NetworkComm {
 public:
  /**
   * Constructor for NetworkComm
   *
   * Initializes internal variables but does not start network communication.
   * Call begin() to start network communication.
   */
  NetworkComm();

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
   * tasks like broadcasting board presence. It should be called in the Arduino
   * loop().
   */
  void update();

  // ==================== Board Discovery & Network Status ====================
  /**
   * Check if the board is connected to the network
   *
   * @return true if connected to WiFi and ESP-NOW is initialized
   */
  bool isConnected();

  /**
   * Check if a specific board is available on the network
   *
   * @param boardId The ID of the board to check
   * @return true if the board has been discovered, false otherwise
   */
  bool isBoardAvailable(const char* boardId);

  /**
   * Get the number of available boards on the network
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
   * When enabled, message delivery will be confirmed with acknowledgements.
   * This increases reliability but uses more bandwidth.
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
   * Verbose logging includes more detailed information than debug logging.
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
   * Register a callback for ESP-NOW send status
   *
   * This will be called whenever an ESP-NOW message is sent, with the status
   * of the transmission (success or failure)
   *
   * @param callback Function to call when a message send status is received
   * @return true if the callback was set successfully
   */
  bool onSendStatus(SendStatusCallback callback);

  /**
   * Register a callback specifically for message delivery failures
   *
   * This will be called only when an ESP-NOW message fails to deliver.
   * For pin control messages, the pin and value will be included in the
   * callback. For other message types, the pin and value will be 0.
   *
   * @param callback Function to call when a message fails to deliver
   * @return true if the callback was set successfully
   */
  bool onSendFailure(SendFailureCallback callback);

  /**
   * Enable or disable automatic retries for pin control messages
   *
   * When enabled, pin control messages that fail to deliver will be
   * automatically retried up to the configured maximum number of retries.
   * This works for all pin control messages, regardless of whether a
   * callback was provided.
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
   * This setting affects all pin control messages when retries are enabled,
   * whether or not they have callbacks registered.
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
   * This setting affects all pin control messages when retries are enabled,
   * whether or not they have callbacks registered.
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
   *
   * Examples:
   * - controlRemotePin("boardA", 13, HIGH)
   *   Simple pin control, no feedback
   *
   * - controlRemotePin("boardA", 13, HIGH, myCallback)
   *   Callback triggered when message is delivered or fails
   *
   * Note: If auto-retries are enabled (using enablePinControlRetries),
   * they will work for all pin control messages, even when no callback is
   * provided.
   */
  bool controlRemotePin(const char* targetBoardId, uint8_t pin, uint8_t value,
                        PinControlConfirmCallback callback = NULL);

  /**
   * Control a pin on a remote board with confirmation
   *
   * This method is maintained for backward compatibility and will be removed in
   * a future version. Please use controlRemotePin with callback instead.
   *
   * @param targetBoardId The ID of the target board
   * @param pin The pin number to control
   * @param value The value to set (HIGH/LOW)
   * @param callback Function to call when the operation completes or times out
   * @return true if the message was sent successfully
   */
  bool controlRemotePinWithConfirmation(const char* targetBoardId, uint8_t pin,
                                        uint8_t value,
                                        PinControlConfirmCallback callback) {
    return controlRemotePin(targetBoardId, pin, value, callback);
  }

  /**
   * Clear all pin control confirmation callbacks
   *
   * This clears both the deprecated global callback and all per-message
   * callbacks.
   *
   * @return true if callbacks were cleared successfully
   */
  bool clearRemotePinConfirmCallback();

  /**
   * Read the value of a pin on a remote board
   *
   * This method sends a request to read a pin on a remote board and provides
   * the result through a callback. Since ESP-NOW communication is asynchronous,
   * the pin value cannot be returned directly.
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
   * This method sends a request to read a pin on a remote board and returns the
   * result immediately.
   *
   * @param targetBoardId The ID of the target board
   * @param pin The pin number to read
   * @return The pin value
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
   * This method allows specific pin control from specific boards.
   * Consider using handlePinControl for more flexible pin control handling.
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
   * When this is set up, the board will respond to pin read requests by reading
   * the requested pin value and sending it back to the requester.
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
   * Broadcast the state of a pin to all boards on the network
   *
   * This is useful for informing other boards about the state of a pin,
   * without expecting them to control it.
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
   * Forward serial data to all boards on the network
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
  uint8_t _macAddress[6];
  bool _isConnected;
  bool _acknowledgementsEnabled;
  bool _debugLoggingEnabled;
  bool _verboseLoggingEnabled;
  bool _pinControlRetriesEnabled;
  uint8_t _pinControlMaxRetries;
  uint16_t _pinControlRetryDelay;

  // Message tracking for acknowledgements
  static const int MAX_TRACKED_MESSAGES = 10;
  struct MessageTrack {
    char messageId[37];  // UUID string length
    char targetBoard[32];
    bool acknowledged;
    uint32_t sentTime;
    bool active;
    uint8_t messageType;  // Store the message type
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

  // ESP-NOW send callback
  static void onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status);
  void handleSendStatus(const uint8_t* mac_addr, esp_now_send_status_t status);
  SendStatusCallback _sendStatusCallback;
  SendFailureCallback _sendFailureCallback;

  // Message acknowledgement handling
  /**
   * Send an acknowledgement for a received message
   *
   * @param sender The board ID that sent the original message
   * @param messageId The ID of the message to acknowledge
   */
  void sendAcknowledgement(const char* sender, const char* messageId);

  /**
   * Handle an incoming acknowledgement message
   *
   * @param sender The board ID that sent the acknowledgement
   * @param messageId The ID of the message being acknowledged
   */
  void handleAcknowledgement(const char* sender, const char* messageId);

  /**
   * Generate a unique message ID for tracking
   *
   * @param buffer Character buffer to store the generated ID (should be at
   * least 37 bytes)
   */
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
  /**
   * Broadcast this board's presence to the network
   *
   * Used for discovery by other boards.
   */
  void broadcastPresence();

  /**
   * Handle a discovery message from another board
   *
   * @param senderId The ID of the board that sent the discovery message
   * @param senderMac The MAC address of the board that sent the discovery
   * message
   */
  void handleDiscovery(const char* senderId, const uint8_t* senderMac);

  /**
   * Add a peer to the list of known boards
   *
   * @param boardId The ID of the board to add
   * @param macAddress The MAC address of the board to add
   * @return true if the peer was added successfully
   */
  bool addPeer(const char* boardId, const uint8_t* macAddress);

  /**
   * Get the MAC address for a board ID
   *
   * @param boardId The ID of the board to look up
   * @param macAddress Buffer to store the MAC address (must be at least 6
   * bytes)
   * @return true if the board was found and the MAC address was written
   */
  bool getMacForBoardId(const char* boardId, uint8_t* macAddress);

  /**
   * Find a board ID from a MAC address
   *
   * @param macAddress The MAC address to look up
   * @param boardId Buffer to store the board ID (should be at least 32 bytes)
   * @return true if the MAC address was found and the board ID was written
   */
  bool getBoardIdForMac(const uint8_t* macAddress, char* boardId);

  // Message handling
  /**
   * ESP-NOW data received callback (static)
   *
   * Called by ESP-NOW when data is received. This function runs in interrupt
   * context and should do minimal processing.
   *
   * @param mac The MAC address of the sender
   * @param data The received data
   * @param len The length of the received data
   */
  static void onDataReceived(const uint8_t* mac, const uint8_t* data, int len);

  /**
   * Process an incoming message
   *
   * @param mac The MAC address of the sender
   * @param data The received data
   * @param len The length of the received data
   */
  void processIncomingMessage(const uint8_t* mac, const uint8_t* data, int len);

  /**
   * Send a message to a specific board
   *
   * @param targetBoard The ID of the target board
   * @param messageType The type of message to send
   * @param doc The JSON object containing the message payload
   * @return true if the message was sent successfully
   */
  bool sendMessage(const char* targetBoard, uint8_t messageType,
                   const JsonObject& doc);

  /**
   * Broadcast a message to all boards on the network
   *
   * @param messageType The type of message to broadcast
   * @param doc The JSON object containing the message payload
   * @return true if the broadcast was sent successfully
   */
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
  uint8_t (*_pinReadCallback)(
      uint8_t pin);  // Callback for handling pin read requests

  // Internal helper functions
  uint32_t _lastDiscoveryBroadcast;

  /**
   * Perform network discovery
   *
   * Sends discovery messages and processes discovery responses.
   */
  void performDiscovery();

  // Structure for pin read responses queue
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