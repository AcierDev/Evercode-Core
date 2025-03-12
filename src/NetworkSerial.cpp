/**
 * NetworkSerial.cpp - Serial data forwarding functionality for ESP32 network
 * communication Created as part of the NetworkComm library refactoring
 */

#include "NetworkSerial.h"

// Constructor
NetworkSerial::NetworkSerial(NetworkCore& core) : _core(core) {
  _serialDataCallback = NULL;
  _autoForwardingEnabled = false;
  _serialBufferIndex = 0;
  _lastSerialRead = 0;
}

bool NetworkSerial::begin() {
  // No specific initialization needed
  return true;
}

bool NetworkSerial::forwardSerialData(const char* data) {
  if (!_core.isConnected()) return false;
  if (!data) return false;

  // Prepare the message
  StaticJsonDocument<256> doc;
  doc["data"] = data;

  // Broadcast the message
  return _core.broadcastMessage(MSG_TYPE_SERIAL_DATA, doc.as<JsonObject>());
}

bool NetworkSerial::receiveSerialData(SerialDataCallback callback) {
  _serialDataCallback = callback;
  return true;
}

bool NetworkSerial::stopReceivingSerialData() {
  _serialDataCallback = NULL;
  return true;
}

bool NetworkSerial::handleSerialDataMessage(const char* sender,
                                            const char* data) {
  if (!sender || !data) return false;

  // Call the callback if registered
  if (_serialDataCallback) {
    _serialDataCallback(sender, data);
    return true;
  }

  return false;
}

bool NetworkSerial::enableAutoForwarding(bool enable) {
  _autoForwardingEnabled = enable;

  // Reset buffer when enabling
  if (enable) {
    _serialBufferIndex = 0;
    _lastSerialRead = millis();
  }

  return true;
}

void NetworkSerial::update() {
  // Only process if auto-forwarding is enabled and we're connected
  if (!_autoForwardingEnabled || !_core.isConnected()) return;

  uint32_t currentTime = millis();

  // Check for data on Serial
  while (Serial.available() && _serialBufferIndex < MAX_SERIAL_DATA_SIZE - 1) {
    char c = Serial.read();
    _serialBuffer[_serialBufferIndex++] = c;
    _lastSerialRead = currentTime;

    // Send if newline or buffer nearly full
    if (c == '\n' || c == '\r' ||
        _serialBufferIndex >= MAX_SERIAL_DATA_SIZE - 2) {
      _serialBuffer[_serialBufferIndex] = '\0';
      forwardSerialData(_serialBuffer);
      _serialBufferIndex = 0;
    }
  }

  // Also send if we have data but haven't received anything for a while
  if (_serialBufferIndex > 0 && (currentTime - _lastSerialRead > 500)) {
    _serialBuffer[_serialBufferIndex] = '\0';
    forwardSerialData(_serialBuffer);
    _serialBufferIndex = 0;
  }
}