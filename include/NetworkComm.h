/**
 * NetworkComm.h - Library for communication between ESP32 boards
 * Created by Claude, 2023
 * Modified to use ESP-NOW for direct communication
 * Refactored into a modular architecture in 2024
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

#include "NetworkCore.h"
#include "NetworkDiagnostics.h"
#include "NetworkDiscovery.h"
#include "NetworkMessaging.h"
#include "NetworkPinControl.h"
#include "NetworkSerial.h"

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
// New callback type for ESP-NOW send status
typedef void (*SendStatusCallback)(const char* targetBoardId,
                                   uint8_t messageType, bool success);
// New callback specifically for send failures
typedef void (*SendFailureCallback)(const char* targetBoardId,
                                    uint8_t messageType, uint8_t pin,
                                    uint8_t value);

class NetworkComm {
 public:
  /**
   * Constructor for NetworkComm
   *
   * Initializes internal variables but does not start network communication.
   * Call begin() to start network communication.
   */
  NetworkComm();

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

  // ==================== Board Discovery & Network Status ====================
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
                                        PinControlConfirmCallback callback);

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
  // Core network instance
  NetworkCore _core;

  // Module instances
  NetworkDiscovery _discovery;
  NetworkPinControl _pinControl;
  NetworkMessaging _messaging;
  NetworkSerial _serial;
  NetworkDiagnostics _diagnostics;
};

#endif