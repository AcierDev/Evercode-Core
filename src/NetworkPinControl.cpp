/**
 * NetworkPinControl.cpp - Remote pin control functionality for ESP32 network
 * communication Created as part of the NetworkComm library refactoring
 */

#include "NetworkPinControl.h"

// Constructor
NetworkPinControl::NetworkPinControl(NetworkCore& core) : _core(core) {
  _globalPinChangeCallback = NULL;
  _pinControlConfirmCallback = NULL;
  _pinSubscriptionCount = 0;

  // Initialize subscriptions
  for (int i = 0; i < MAX_PIN_SUBSCRIPTIONS; i++) {
    _pinSubscriptions[i].active = false;
  }
}

bool NetworkPinControl::begin() {
  // No specific initialization needed
  return true;
}

// ==================== Remote Pin Control (Controller Side)
// ====================

bool NetworkPinControl::controlRemotePin(const char* targetBoardId, uint8_t pin,
                                         uint8_t value,
                                         PinControlConfirmCallback callback) {
  if (!_core.isConnected()) return false;

  // Prepare the message
  StaticJsonDocument<64> doc;
  doc["pin"] = pin;
  doc["value"] = value;

  // Store pin details for callbacks
  int freeSlot = -1;
  for (int i = 0; i < NetworkCore::MAX_TRACKED_MESSAGES; i++) {
    if (!_core._trackedMessages[i].active) {
      freeSlot = i;
      break;
    }
  }

  if (freeSlot != -1) {
    _core._trackedMessages[freeSlot].pin = pin;
    _core._trackedMessages[freeSlot].value = value;
    _core._trackedMessages[freeSlot].confirmCallback = (void*)callback;
  }

  // Send the message
  return _core.sendMessage(targetBoardId, MSG_TYPE_PIN_CONTROL,
                           doc.as<JsonObject>());
}

bool NetworkPinControl::clearRemotePinConfirmCallback() {
  _pinControlConfirmCallback = NULL;

  // Also clear all pin control callbacks in the tracked messages
  for (int i = 0; i < NetworkCore::MAX_TRACKED_MESSAGES; i++) {
    if (_core._trackedMessages[i].active &&
        _core._trackedMessages[i].messageType == MSG_TYPE_PIN_CONTROL) {
      _core._trackedMessages[i].confirmCallback = NULL;
    }
  }

  return true;
}

uint8_t NetworkPinControl::readRemotePin(const char* targetBoardId,
                                         uint8_t pin) {
  // This would need to be implemented with a request/response pattern
  // Currently not fully implemented - always returns 0
  return 0;
}

// ==================== Remote Pin Control (Responder Side) ====================

bool NetworkPinControl::handlePinControl(PinChangeCallback callback) {
  _globalPinChangeCallback = callback;
  return true;
}

bool NetworkPinControl::stopHandlingPinControl() {
  _globalPinChangeCallback = NULL;

  // Also clear pin control subscriptions
  for (int i = 0; i < MAX_PIN_SUBSCRIPTIONS; i++) {
    if (_pinSubscriptions[i].active &&
        _pinSubscriptions[i].type == MSG_TYPE_PIN_CONTROL) {
      _pinSubscriptions[i].active = false;
    }
  }

  return true;
}

bool NetworkPinControl::acceptPinControlFrom(const char* controllerBoardId,
                                             uint8_t pin,
                                             PinChangeCallback callback) {
  if (!_core.isConnected()) return false;

  // Find a free subscription slot
  int slot = findFreePinSubscriptionSlot();
  if (slot == -1) return false;  // No free slots

  // Store the subscription
  strncpy(_pinSubscriptions[slot].targetBoard, controllerBoardId,
          sizeof(_pinSubscriptions[slot].targetBoard) - 1);
  _pinSubscriptions[slot]
      .targetBoard[sizeof(_pinSubscriptions[slot].targetBoard) - 1] = '\0';
  _pinSubscriptions[slot].pin = pin;
  _pinSubscriptions[slot].type = MSG_TYPE_PIN_CONTROL;
  _pinSubscriptions[slot].callback = callback;
  _pinSubscriptions[slot].active = true;

  if (_pinSubscriptionCount < MAX_PIN_SUBSCRIPTIONS) _pinSubscriptionCount++;

  // Send subscription request to the controller
  StaticJsonDocument<64> doc;
  doc["pin"] = pin;

  return _core.sendMessage(controllerBoardId, MSG_TYPE_PIN_SUBSCRIBE,
                           doc.as<JsonObject>());
}

bool NetworkPinControl::stopAcceptingPinControlFrom(
    const char* controllerBoardId, uint8_t pin) {
  if (!_core.isConnected()) return false;

  // Find and remove the matching subscription
  int index = -1;
  if (findMatchingPinSubscription(controllerBoardId, pin, MSG_TYPE_PIN_CONTROL,
                                  index)) {
    _pinSubscriptions[index].active = false;
    return true;
  }

  return false;  // Subscription not found
}

// ==================== Pin State Broadcasting ====================

bool NetworkPinControl::broadcastPinState(uint8_t pin, uint8_t value) {
  if (!_core.isConnected()) return false;

  // Prepare the message
  StaticJsonDocument<64> doc;
  doc["pin"] = pin;
  doc["value"] = value;

  // Broadcast the pin state
  return _core.broadcastMessage(MSG_TYPE_PIN_PUBLISH, doc.as<JsonObject>());
}

bool NetworkPinControl::listenForPinStateFrom(const char* broadcasterBoardId,
                                              uint8_t pin,
                                              PinChangeCallback callback) {
  if (!_core.isConnected()) return false;

  // Find a free subscription slot
  int slot = findFreePinSubscriptionSlot();
  if (slot == -1) return false;  // No free slots

  // Store the subscription
  strncpy(_pinSubscriptions[slot].targetBoard, broadcasterBoardId,
          sizeof(_pinSubscriptions[slot].targetBoard) - 1);
  _pinSubscriptions[slot]
      .targetBoard[sizeof(_pinSubscriptions[slot].targetBoard) - 1] = '\0';
  _pinSubscriptions[slot].pin = pin;
  _pinSubscriptions[slot].type = MSG_TYPE_PIN_PUBLISH;
  _pinSubscriptions[slot].callback = callback;
  _pinSubscriptions[slot].active = true;

  if (_pinSubscriptionCount < MAX_PIN_SUBSCRIPTIONS) _pinSubscriptionCount++;

  return true;
}

bool NetworkPinControl::stopListeningForPinStateFrom(
    const char* broadcasterBoardId, uint8_t pin) {
  if (!_core.isConnected()) return false;

  // Find and remove the matching subscription
  int index = -1;
  if (findMatchingPinSubscription(broadcasterBoardId, pin, MSG_TYPE_PIN_PUBLISH,
                                  index)) {
    _pinSubscriptions[index].active = false;
    return true;
  }

  return false;  // Subscription not found
}

// ==================== Message Handlers ====================

bool NetworkPinControl::handlePinControlMessage(const char* sender, uint8_t pin,
                                                uint8_t value,
                                                const char* messageId) {
  // Send acknowledgement if needed
  if (messageId != NULL && _core.isAcknowledgementsEnabled()) {
    _core.sendAcknowledgement(sender, messageId);
  }

  bool pinHandled = false;

  // First, check if there's a global callback
  if (_globalPinChangeCallback != NULL) {
    _globalPinChangeCallback(sender, pin, value);
    pinHandled = true;
  }

  // Next, check for specific subscriptions
  int index = -1;
  if (findMatchingPinSubscription(sender, pin, MSG_TYPE_PIN_CONTROL, index)) {
    if (_pinSubscriptions[index].callback != NULL) {
      _pinSubscriptions[index].callback(sender, pin, value);
      pinHandled = true;
    }
  }

  // If no callback handled it, set the pin directly (if it's valid)
  if (!pinHandled && pin < NUM_DIGITAL_PINS) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value);
    pinHandled = true;
  }

  return pinHandled;
}

bool NetworkPinControl::handlePinStateMessage(const char* sender, uint8_t pin,
                                              uint8_t value) {
  bool pinHandled = false;

  // First, check if there's a global callback
  if (_globalPinChangeCallback != NULL) {
    _globalPinChangeCallback(sender, pin, value);
    pinHandled = true;
  }

  // Next, check for specific subscriptions
  int index = -1;
  if (findMatchingPinSubscription(sender, pin, MSG_TYPE_PIN_PUBLISH, index)) {
    if (_pinSubscriptions[index].callback != NULL) {
      _pinSubscriptions[index].callback(sender, pin, value);
      pinHandled = true;
    }
  }

  return pinHandled;
}

// ==================== Helper Methods ====================

int NetworkPinControl::findFreePinSubscriptionSlot() {
  for (int i = 0; i < MAX_PIN_SUBSCRIPTIONS; i++) {
    if (!_pinSubscriptions[i].active) {
      return i;
    }
  }
  return -1;  // No free slots
}

bool NetworkPinControl::findMatchingPinSubscription(const char* boardId,
                                                    uint8_t pin, uint8_t type,
                                                    int& index) {
  for (int i = 0; i < MAX_PIN_SUBSCRIPTIONS; i++) {
    if (_pinSubscriptions[i].active && _pinSubscriptions[i].type == type &&
        _pinSubscriptions[i].pin == pin &&
        strcmp(_pinSubscriptions[i].targetBoard, boardId) == 0) {
      index = i;
      return true;
    }
  }
  return false;  // No match found
}