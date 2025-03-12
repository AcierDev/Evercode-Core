/**
 * NetworkComm.cpp - Library for communication between ESP32 boards
 * Created by Claude, 2023
 * Modified to use ESP-NOW for direct communication
 * Refactored into a modular architecture in 2024
 */

#include "NetworkComm.h"

// Constructor
NetworkComm::NetworkComm()
    : _core(),
      _discovery(_core),
      _pinControl(_core),
      _messaging(_core),
      _serial(_core),
      _diagnostics(_core) {
  // All initialization is done in begin()
}

// Initialize with WiFi
bool NetworkComm::begin(const char* ssid, const char* password,
                        const char* boardId) {
  // Initialize the core network
  if (!_core.begin(ssid, password, boardId)) {
    return false;
  }

  // Initialize all modules
  _discovery.begin();
  _pinControl.begin();
  _messaging.begin();
  _serial.begin();
  _diagnostics.begin();

  return true;
}

// Main loop function - must be called in loop()
void NetworkComm::update() {
  // Update core and all modules
  _core.update();
  _discovery.update();
  _diagnostics.update();
  _serial.update();  // Only does work if auto-forwarding is enabled
}

// ==================== Network Status ====================

bool NetworkComm::isConnected() { return _core.isConnected(); }

// ==================== Board Discovery ====================

bool NetworkComm::isBoardAvailable(const char* boardId) {
  return _discovery.isBoardAvailable(boardId);
}

int NetworkComm::getAvailableBoardsCount() {
  return _discovery.getAvailableBoardsCount();
}

String NetworkComm::getAvailableBoardName(int index) {
  return _discovery.getAvailableBoardName(index);
}

bool NetworkComm::onBoardDiscovered(DiscoveryCallback callback) {
  return _discovery.onBoardDiscovered(callback);
}

// ==================== Debug & Diagnostic Features ====================

bool NetworkComm::enableMessageAcknowledgements(bool enable) {
  return _core.enableMessageAcknowledgements(enable);
}

bool NetworkComm::isAcknowledgementsEnabled() {
  return _core.isAcknowledgementsEnabled();
}

bool NetworkComm::enableDebugLogging(bool enable) {
  return _diagnostics.enableDebugLogging(enable);
}

bool NetworkComm::isDebugLoggingEnabled() {
  return _diagnostics.isDebugLoggingEnabled();
}

bool NetworkComm::enableVerboseLogging(bool enable) {
  return _diagnostics.enableVerboseLogging(enable);
}

bool NetworkComm::isVerboseLoggingEnabled() {
  return _diagnostics.isVerboseLoggingEnabled();
}

bool NetworkComm::onSendStatus(SendStatusCallback callback) {
  return _core.onSendStatus(callback);
}

bool NetworkComm::onSendFailure(SendFailureCallback callback) {
  return _core.onSendFailure(callback);
}

// ==================== Remote Pin Control (Controller Side)
// ====================

bool NetworkComm::controlRemotePin(const char* targetBoardId, uint8_t pin,
                                   uint8_t value,
                                   PinControlConfirmCallback callback) {
  return _pinControl.controlRemotePin(targetBoardId, pin, value, callback);
}

bool NetworkComm::controlRemotePinWithConfirmation(
    const char* targetBoardId, uint8_t pin, uint8_t value,
    PinControlConfirmCallback callback) {
  return _pinControl.controlRemotePinWithConfirmation(targetBoardId, pin, value,
                                                      callback);
}

bool NetworkComm::clearRemotePinConfirmCallback() {
  return _pinControl.clearRemotePinConfirmCallback();
}

uint8_t NetworkComm::readRemotePin(const char* targetBoardId, uint8_t pin) {
  return _pinControl.readRemotePin(targetBoardId, pin);
}

// ==================== Remote Pin Control (Responder Side) ====================

bool NetworkComm::handlePinControl(PinChangeCallback callback) {
  return _pinControl.handlePinControl(callback);
}

bool NetworkComm::stopHandlingPinControl() {
  return _pinControl.stopHandlingPinControl();
}

bool NetworkComm::acceptPinControlFrom(const char* controllerBoardId,
                                       uint8_t pin,
                                       PinChangeCallback callback) {
  return _pinControl.acceptPinControlFrom(controllerBoardId, pin, callback);
}

bool NetworkComm::stopAcceptingPinControlFrom(const char* controllerBoardId,
                                              uint8_t pin) {
  return _pinControl.stopAcceptingPinControlFrom(controllerBoardId, pin);
}

// ==================== Pin State Broadcasting ====================

bool NetworkComm::broadcastPinState(uint8_t pin, uint8_t value) {
  return _pinControl.broadcastPinState(pin, value);
}

bool NetworkComm::listenForPinStateFrom(const char* broadcasterBoardId,
                                        uint8_t pin,
                                        PinChangeCallback callback) {
  return _pinControl.listenForPinStateFrom(broadcasterBoardId, pin, callback);
}

bool NetworkComm::stopListeningForPinStateFrom(const char* broadcasterBoardId,
                                               uint8_t pin) {
  return _pinControl.stopListeningForPinStateFrom(broadcasterBoardId, pin);
}

// ==================== Topic-based Messaging ====================

bool NetworkComm::publishTopic(const char* topic, const char* message) {
  return _messaging.publishTopic(topic, message);
}

bool NetworkComm::subscribeTopic(const char* topic, MessageCallback callback) {
  return _messaging.subscribeTopic(topic, callback);
}

bool NetworkComm::unsubscribeTopic(const char* topic) {
  return _messaging.unsubscribeTopic(topic);
}

// ==================== Serial Data Forwarding ====================

bool NetworkComm::forwardSerialData(const char* data) {
  return _serial.forwardSerialData(data);
}

bool NetworkComm::receiveSerialData(SerialDataCallback callback) {
  return _serial.receiveSerialData(callback);
}

bool NetworkComm::stopReceivingSerialData() {
  return _serial.stopReceivingSerialData();
}

// ==================== Direct Messaging ====================

bool NetworkComm::sendMessageToBoardId(const char* targetBoardId,
                                       const char* message) {
  return _messaging.sendMessageToBoardId(targetBoardId, message);
}

bool NetworkComm::receiveMessagesFromBoards(MessageCallback callback) {
  return _messaging.receiveMessagesFromBoards(callback);
}
