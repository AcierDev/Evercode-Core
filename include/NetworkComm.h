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

  // ==================== Remote Pin Control (Controller Side)
  // ====================
  /**
   * Control a pin on a remote board
   *
   * @param targetBoardId The ID of the target board
   * @param pin The pin number to control
   * @param value The value to set (HIGH/LOW)
   * @param callback Optional callback to be called when the operation completes
   * or times out
   * @param requireConfirmation If true, waits for confirmation from the remote
   * board
   * @return true if the message was sent successfully
   *
   * Examples:
   * - controlRemotePin("boardA", 13, HIGH)
   *   Simple pin control without callback or confirmation
   *
   * - controlRemotePin("boardA", 13, HIGH, myCallback)
   *   Pin control with callback but no explicit confirmation
   *
   * - controlRemotePin("boardA", 13, HIGH, myCallback, true)
   *   Full pin control with callback and confirmation
   */
  bool controlRemotePin(const char* targetBoardId, uint8_t pin, uint8_t value,
                        PinControlConfirmCallback callback = NULL,
                        bool requireConfirmation = false);

  /**
   * Control a pin on a remote board with confirmation (DEPRECATED)
   *
   * @param targetBoardId The ID of the target board
   * @param pin The pin number to control
   * @param value The value to set (HIGH/LOW)
   * @param callback Function to call when the operation completes or times out
   * @return true if the message was sent successfully
   *
   * @deprecated Use controlRemotePin with callback and requireConfirmation=true
   * instead
   */
  bool controlRemotePinWithConfirmation(const char* targetBoardId, uint8_t pin,
                                        uint8_t value,
                                        PinControlConfirmCallback callback) {
    return controlRemotePin(targetBoardId, pin, value, callback, true);
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
   * Note: This method is not fully implemented and always returns 0.
   * A proper implementation would require a request/response pattern.
   *
   * @param targetBoardId The ID of the target board
   * @param pin The pin number to read
   * @return The pin value (currently always returns 0)
   */
  uint8_t readRemotePin(const char* targetBoardId, uint8_t pin);

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
   * Accept pin control from a specific board for a specific pin (DEPRECATED)
   *
   * @param controllerBoardId The ID of the board to accept control from
   * @param pin The pin to allow control of
   * @param callback Function to call when pin control is received
   * @return true if the subscription was added successfully
   *
   * @deprecated Use handlePinControl instead
   */
  bool acceptPinControlFrom(const char* controllerBoardId, uint8_t pin,
                            PinChangeCallback callback);

  /**
   * Stop accepting pin control from a specific board for a specific pin
   * (DEPRECATED)
   *
   * @param controllerBoardId The ID of the board to stop accepting control from
   * @param pin The pin to stop allowing control of
   * @return true if the subscription was removed successfully
   *
   * @deprecated Use stopHandlingPinControl instead
   */
  bool stopAcceptingPinControlFrom(const char* controllerBoardId, uint8_t pin);

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

  // ==================== Deprecated API (for backward compatibility)
  // ==================== These methods are kept for backward compatibility but
  // will be removed in future versions

  // Pin control (deprecated)
  /**
   * Set a pin value on a remote board (DEPRECATED)
   *
   * @param targetBoard The ID of the target board
   * @param pin The pin number to control
   * @param value The value to set (HIGH/LOW)
   * @return true if the message was sent successfully
   *
   * @deprecated Use controlRemotePin instead
   */
  bool setPinValue(const char* targetBoard, uint8_t pin, uint8_t value) {
    return controlRemotePin(targetBoard, pin, value);
  }

  /**
   * Set a pin value on a remote board with confirmation (DEPRECATED)
   *
   * @param targetBoard The ID of the target board
   * @param pin The pin number to control
   * @param value The value to set (HIGH/LOW)
   * @param callback Function to call when the operation completes or times out
   * @return true if the message was sent successfully
   *
   * @deprecated Use controlRemotePin with callback and requireConfirmation=true
   * instead
   */
  bool setPinValueWithConfirmation(const char* targetBoard, uint8_t pin,
                                   uint8_t value,
                                   PinControlConfirmCallback callback) {
    return controlRemotePinWithConfirmation(targetBoard, pin, value, callback);
  }

  /**
   * Clear the pin control confirmation callback (DEPRECATED)
   *
   * @return true if the callback was cleared successfully
   *
   * @deprecated Use clearRemotePinConfirmCallback instead
   */
  bool clearPinControlConfirmCallback() {
    return clearRemotePinConfirmCallback();
  }

  /**
   * Get the value of a pin on a remote board (DEPRECATED)
   *
   * @param targetBoard The ID of the target board
   * @param pin The pin number to read
   * @return The pin value (currently always returns 0)
   *
   * @deprecated Use readRemotePin instead
   */
  uint8_t getPinValue(const char* targetBoard, uint8_t pin) {
    return readRemotePin(targetBoard, pin);
  }

  // Pin subscription (deprecated)
  /**
   * Subscribe to pin changes on a remote board (DEPRECATED)
   *
   * @param targetBoard The ID of the board to subscribe to
   * @param pin The pin to subscribe to
   * @param callback Function to call when the pin changes
   * @return true if the subscription was added successfully
   *
   * @deprecated Use acceptPinControlFrom instead
   */
  bool subscribeToPinChange(const char* targetBoard, uint8_t pin,
                            PinChangeCallback callback) {
    return acceptPinControlFrom(targetBoard, pin, callback);
  }

  /**
   * Unsubscribe from pin changes on a remote board (DEPRECATED)
   *
   * @param targetBoard The ID of the board to unsubscribe from
   * @param pin The pin to unsubscribe from
   * @return true if the subscription was removed successfully
   *
   * @deprecated Use stopAcceptingPinControlFrom instead
   */
  bool unsubscribeFromPinChange(const char* targetBoard, uint8_t pin) {
    return stopAcceptingPinControlFrom(targetBoard, pin);
  }

  // Message pub/sub (deprecated)
  /**
   * Publish a message to a topic (DEPRECATED)
   *
   * @param topic The topic to publish to
   * @param message The message to publish
   * @return true if the message was sent successfully
   *
   * @deprecated Use publishTopic instead
   */
  bool publish(const char* topic, const char* message) {
    return publishTopic(topic, message);
  }

  /**
   * Subscribe to a topic (DEPRECATED)
   *
   * @param topic The topic to subscribe to
   * @param callback Function to call when a message is received on this topic
   * @return true if the subscription was added successfully
   *
   * @deprecated Use subscribeTopic instead
   */
  bool subscribe(const char* topic, MessageCallback callback) {
    return subscribeTopic(topic, callback);
  }

  /**
   * Unsubscribe from a topic (DEPRECATED)
   *
   * @param topic The topic to unsubscribe from
   * @return true if the subscription was removed successfully
   *
   * @deprecated Use unsubscribeTopic instead
   */
  bool unsubscribe(const char* topic) { return unsubscribeTopic(topic); }

  // Serial data pub/sub (deprecated)
  /**
   * Publish serial data (DEPRECATED)
   *
   * @param data The data to publish
   * @return true if the data was sent successfully
   *
   * @deprecated Use forwardSerialData instead
   */
  bool publishSerialData(const char* data) { return forwardSerialData(data); }

  /**
   * Subscribe to serial data (DEPRECATED)
   *
   * @param callback Function to call when serial data is received
   * @return true if the callback was set successfully
   *
   * @deprecated Use receiveSerialData instead
   */
  bool subscribeToSerialData(SerialDataCallback callback) {
    return receiveSerialData(callback);
  }

  /**
   * Unsubscribe from serial data (DEPRECATED)
   *
   * @return true if the callback was cleared successfully
   *
   * @deprecated Use stopReceivingSerialData instead
   */
  bool unsubscribeFromSerialData() { return stopReceivingSerialData(); }

  // Direct messaging (deprecated)
  /**
   * Send a direct message to a board (DEPRECATED)
   *
   * @param targetBoard The ID of the board to send the message to
   * @param message The message to send
   * @return true if the message was sent successfully
   *
   * @deprecated Use sendMessageToBoardId instead
   */
  bool sendDirectMessage(const char* targetBoard, const char* message) {
    return sendMessageToBoardId(targetBoard, message);
  }

  /**
   * Set a callback for direct messages (DEPRECATED)
   *
   * @param callback Function to call when a direct message is received
   * @return true if the callback was set successfully
   *
   * @deprecated Use receiveMessagesFromBoards instead
   */
  bool setDirectMessageCallback(MessageCallback callback) {
    return receiveMessagesFromBoards(callback);
  }

  // Board Discovery (deprecated)
  /**
   * Set a callback for board discovery (DEPRECATED)
   *
   * @param callback Function to call when a new board is discovered
   * @return true if the callback was set successfully
   *
   * @deprecated Use onBoardDiscovered instead
   */
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
  bool _verboseLoggingEnabled;

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
  };

  MessageTrack _trackedMessages[MAX_TRACKED_MESSAGES];
  int _trackedMessageCount;

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

  // Internal helper functions
  uint32_t _lastDiscoveryBroadcast;

  /**
   * Perform network discovery
   *
   * Sends discovery messages and processes discovery responses.
   */
  void performDiscovery();
};

#endif