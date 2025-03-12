/**
 * NetworkPinControl.h - Remote pin control functionality for ESP32 network
 * communication Created as part of the NetworkComm library refactoring
 *
 * This class handles controlling pins on remote ESP32 boards and
 * receiving pin control commands from other boards.
 */

#ifndef NetworkPinControl_h
#define NetworkPinControl_h

#include "NetworkCore.h"

// Maximum number of subscriptions
#define MAX_PIN_SUBSCRIPTIONS 20

// Pin control confirmation timeout
#define PIN_CONTROL_CONFIRM_TIMEOUT 5000  // 5 seconds

// Callback function types
typedef void (*PinChangeCallback)(const char* sender, uint8_t pin,
                                  uint8_t value);
typedef void (*PinControlConfirmCallback)(const char* sender, uint8_t pin,
                                          uint8_t value, bool success);

class NetworkPinControl {
 public:
  /**
   * Constructor for NetworkPinControl
   *
   * @param core Reference to the NetworkCore instance
   */
  NetworkPinControl(NetworkCore& core);

  /**
   * Initialize the pin control service
   *
   * @return true if initialization was successful
   */
  bool begin();

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
   * This method is maintained for backward compatibility
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
   * @return true if callbacks were cleared successfully
   */
  bool clearRemotePinConfirmCallback();

  /**
   * Read the value of a pin on a remote board
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
   * @param callback Optional callback to process pin control requests. If NULL,
   * the library will automatically set pins directly.
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

  // ==================== Pin State Broadcasting ====================
  /**
   * Broadcast the state of a pin to all boards on the network
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

  /**
   * Handle a pin control message
   * Called internally by NetworkCore
   *
   * @param sender The ID of the board that sent the message
   * @param pin The pin to control
   * @param value The value to set
   * @param messageId Optional message ID for acknowledgements
   * @return true if the message was handled successfully
   */
  bool handlePinControlMessage(const char* sender, uint8_t pin, uint8_t value,
                               const char* messageId = NULL);

  /**
   * Handle a pin state broadcast message
   * Called internally by NetworkCore
   *
   * @param sender The ID of the board that sent the message
   * @param pin The pin being reported
   * @param value The pin value
   * @return true if the message was handled successfully
   */
  bool handlePinStateMessage(const char* sender, uint8_t pin, uint8_t value);

 private:
  // Reference to the core network instance
  NetworkCore& _core;

  // Global pin change callback
  PinChangeCallback _globalPinChangeCallback;

  // Legacy pin control confirm callback (for backward compatibility)
  PinControlConfirmCallback _pinControlConfirmCallback;

  // Subscription management for pin control
  struct PinSubscription {
    char targetBoard[32];
    uint8_t pin;
    uint8_t type;  // MSG_TYPE_PIN_SUBSCRIBE or MSG_TYPE_PIN_PUBLISH
    PinChangeCallback callback;
    bool active;
  };

  PinSubscription _pinSubscriptions[MAX_PIN_SUBSCRIPTIONS];
  int _pinSubscriptionCount;

  // Helper methods
  int findFreePinSubscriptionSlot();
  bool findMatchingPinSubscription(const char* boardId, uint8_t pin,
                                   uint8_t type, int& index);
};

#endif