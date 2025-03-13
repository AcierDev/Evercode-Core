/**
 * PinComm.cpp - Library for UART-based communication between ESP32 boards
 * Updated from direct pin-based communication to use UART
 *
 * This library enables communication between ESP32 boards using
 * UART (Serial) connections for reliable data exchange.
 */

#include "../include/PinComm.h"

#include <Arduino.h>
#include <ArduinoJson.h>

// Static pointer to the current PinComm instance for debug logging
static PinComm* _pinCommInstance = nullptr;

// Debug logging helper function
static void pinCommDebugLog(const char* event, const char* details = nullptr) {
  if (_pinCommInstance != nullptr &&
      _pinCommInstance->isDebugLoggingEnabled()) {
    Serial.print("[PinComm] ");
    Serial.print(event);

    if (details != nullptr) {
      Serial.print(": ");
      Serial.print(details);
    }

    Serial.println();
  }
}

// Global variable to store the PinChangeCallback
static PinChangeCallback g_pinChangeCallback = NULL;

// Static wrapper function to adapt PinChangeCallback to
// PinControlConfirmCallback
static void pinControlConfirmWrapper(const char* sender, uint8_t pin,
                                     uint8_t value, bool success) {
  if (g_pinChangeCallback != NULL) {
    g_pinChangeCallback(sender, pin, value);
  }
}

// Constructor
PinComm::PinComm() {
  // Set the static instance pointer for debug logging
  _pinCommInstance = this;

  _isConnected = false;
  _acknowledgementsEnabled = true;
  _debugLoggingEnabled = false;
  _verboseLoggingEnabled = false;
  _pinControlRetriesEnabled = true;
  _pinControlMaxRetries = DEFAULT_MAX_RETRIES;
  _pinControlRetryDelay = DEFAULT_RETRY_DELAY;

  _serialPort = NULL;
  _baudRate = DEFAULT_BAUD_RATE;

  _receiveBufferIndex = 0;
  _isReceiving = false;
  _isEscaped = false;

  _trackedMessageCount = 0;
  _peerCount = 0;
  _subscriptionCount = 0;
  _queuedResponseCount = 0;

  _directMessageCallback = NULL;
  _serialDataCallback = NULL;
  _discoveryCallback = NULL;
  _pinControlConfirmCallback = NULL;
  _pinReadCallback = NULL;
  _sendStatusCallback = NULL;
  _sendFailureCallback = NULL;

  _lastDiscoveryBroadcast = 0;

  // Initialize tracked messages
  for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
    _trackedMessages[i].active = false;
    _trackedMessages[i].acknowledged = false;
    _trackedMessages[i].retryScheduled = false;
  }

  // Initialize peers
  for (int i = 0; i < MAX_PEERS; i++) {
    _peers[i].active = false;
  }

  // Initialize subscriptions
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    _subscriptions[i].active = false;
  }

  // Initialize queued responses
  for (int i = 0; i < MAX_QUEUED_RESPONSES; i++) {
    _queuedResponses[i].active = false;
  }
}

// Initialize the UART-based communication
bool PinComm::begin(Stream* serialPort, const char* boardId,
                    uint32_t baudRate) {
  if (boardId == NULL || strlen(boardId) == 0 || strlen(boardId) > 31) {
    pinCommDebugLog("Invalid board ID", boardId);
    return false;
  }

  // Store board ID
  strncpy(_boardId, boardId, sizeof(_boardId) - 1);
  _boardId[sizeof(_boardId) - 1] = '\0';

  // Store UART configuration
  _serialPort = serialPort;
  _baudRate = baudRate;

  // If serialPort is NULL, try to use the default Serial port
  // This is primarily for backward compatibility
  if (_serialPort == NULL) {
    pinCommDebugLog(
        "No Serial port provided, using Serial at default baud rate");
    Serial.begin(_baudRate);
    _serialPort = &Serial;
  }

  _isConnected = true;

  pinCommDebugLog("PinComm initialized with UART", _boardId);

  // Broadcast presence to discover other boards
  broadcastPresence();

  return true;
}

// Main loop function that must be called regularly
void PinComm::update() {
  if (!_isConnected) return;

  // Check for incoming messages
  receiveFrame();

  // Process queued responses
  processQueuedResponses();

  // Check for message acknowledgements and timeouts
  uint32_t currentTime = millis();

  // Process tracked messages for timeouts and retries
  for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
    if (_trackedMessages[i].active && !_trackedMessages[i].acknowledged) {
      // Check if message has timed out
      if (currentTime - _trackedMessages[i].sentTime > ACK_TIMEOUT) {
        // Message timed out
        if (_verboseLoggingEnabled) {
          pinCommDebugLog("Message timed out", _trackedMessages[i].messageId);
        }

        // For pin control messages with callbacks, notify of failure
        if (_trackedMessages[i].messageType == MSG_TYPE_PIN_CONTROL &&
            _trackedMessages[i].confirmCallback != NULL) {
          _trackedMessages[i].confirmCallback(_trackedMessages[i].targetBoard,
                                              _trackedMessages[i].pin,
                                              _trackedMessages[i].value, false);
        }

        // Call send failure callback if registered
        if (_sendFailureCallback != NULL) {
          _sendFailureCallback(
              _trackedMessages[i].targetBoard, _trackedMessages[i].messageType,
              _trackedMessages[i].pin, _trackedMessages[i].value);
        }

        // Check if we should retry (for pin control messages)
        if (_pinControlRetriesEnabled &&
            _trackedMessages[i].messageType == MSG_TYPE_PIN_CONTROL &&
            _trackedMessages[i].retryCount < _pinControlMaxRetries) {
          // Schedule retry
          _trackedMessages[i].retryCount++;
          _trackedMessages[i].nextRetryTime =
              currentTime + _pinControlRetryDelay;
          _trackedMessages[i].retryScheduled = true;
          _trackedMessages[i].sentTime = currentTime;  // Reset timeout

          if (_verboseLoggingEnabled) {
            char buffer[100];
            sprintf(buffer, "Scheduling retry %d/%d for message",
                    _trackedMessages[i].retryCount, _pinControlMaxRetries);
            pinCommDebugLog(buffer, _trackedMessages[i].messageId);
          }
        } else {
          // No more retries, mark message as inactive
          _trackedMessages[i].active = false;
        }
      }

      // Check if it's time to retry
      if (_trackedMessages[i].retryScheduled &&
          currentTime >= _trackedMessages[i].nextRetryTime) {
        // Retry sending the message
        if (_verboseLoggingEnabled) {
          char buffer[100];
          sprintf(buffer, "Retrying message (attempt %d/%d)",
                  _trackedMessages[i].retryCount, _pinControlMaxRetries);
          pinCommDebugLog(buffer, _trackedMessages[i].messageId);
        }

        // Create a new JSON document for the retry
        StaticJsonDocument<200> doc;
        JsonObject root = doc.to<JsonObject>();

        // Add message ID and sender
        root["id"] = _trackedMessages[i].messageId;
        root["sender"] = _boardId;

        // For pin control messages, add pin and value
        if (_trackedMessages[i].messageType == MSG_TYPE_PIN_CONTROL) {
          root["pin"] = _trackedMessages[i].pin;
          root["value"] = _trackedMessages[i].value;
        }

        // Send the message
        sendMessage(_trackedMessages[i].targetBoard,
                    _trackedMessages[i].messageType, root);

        // Reset retry flag
        _trackedMessages[i].retryScheduled = false;
      }
    }
  }

  // Periodically broadcast presence for discovery
  if (currentTime - _lastDiscoveryBroadcast > 30000) {  // Every 30 seconds
    broadcastPresence();
    _lastDiscoveryBroadcast = currentTime;
  }
}

// UART-based communication methods
bool PinComm::sendFrame(const uint8_t* data, size_t length) {
  if (!_isConnected || data == NULL || length == 0 || _serialPort == NULL)
    return false;

  if (_verboseLoggingEnabled) {
    pinCommDebugLog("Sending frame", String(length).c_str());
  }

  // Send the start byte
  _serialPort->write(FRAME_START_BYTE);

  // Send the data with byte stuffing
  for (size_t i = 0; i < length; i++) {
    uint8_t byte = data[i];

    // Check if we need to escape this byte
    if (byte == FRAME_START_BYTE || byte == FRAME_END_BYTE ||
        byte == ESCAPE_BYTE) {
      // Send escape byte
      _serialPort->write(ESCAPE_BYTE);
      // Send the escaped byte (XOR with 0x20)
      _serialPort->write(byte ^ 0x20);
    } else {
      // Send the byte as is
      _serialPort->write(byte);
    }
  }

  // Send the end byte
  _serialPort->write(FRAME_END_BYTE);

  // Ensure all bytes are transmitted
  _serialPort->flush();

  return true;
}

bool PinComm::receiveFrame() {
  if (!_isConnected || _serialPort == NULL) return false;

  // Check if there's any data available to read
  while (_serialPort->available() > 0) {
    uint8_t byte = _serialPort->read();

    if (!_isReceiving) {
      // We're not currently receiving a frame, look for start byte
      if (byte == FRAME_START_BYTE) {
        _isReceiving = true;
        _receiveBufferIndex = 0;
        _isEscaped = false;
      }
      // Ignore any other bytes while not receiving
    } else {
      // We're in the middle of receiving a frame
      if (_isEscaped) {
        // This byte was escaped, unescape it
        _receiveBuffer[_receiveBufferIndex++] = byte ^ 0x20;
        _isEscaped = false;
      } else if (byte == ESCAPE_BYTE) {
        // Next byte is escaped
        _isEscaped = true;
      } else if (byte == FRAME_END_BYTE) {
        // End of frame
        _isReceiving = false;

        // Process the received frame
        processIncomingMessage(_receiveBuffer, _receiveBufferIndex);
        return true;
      } else if (byte == FRAME_START_BYTE) {
        // Unexpected start byte, restart frame
        _receiveBufferIndex = 0;
      } else {
        // Regular data byte
        _receiveBuffer[_receiveBufferIndex++] = byte;
      }

      // Check if we've reached the buffer limit
      if (_receiveBufferIndex >= MAX_PIN_DATA_SIZE) {
        _isReceiving = false;
        return false;
      }
    }
  }

  return false;
}

// Check if the board is connected and communication is operational
bool PinComm::isConnected() { return _isConnected; }

// Check if a specific board is available
bool PinComm::isBoardAvailable(const char* boardId) {
  if (!_isConnected || boardId == NULL) return false;

  for (int i = 0; i < _peerCount; i++) {
    if (_peers[i].active && strcmp(_peers[i].boardId, boardId) == 0) {
      return true;
    }
  }

  return false;
}

// Get the number of available boards
int PinComm::getAvailableBoardsCount() {
  if (!_isConnected) return 0;

  int count = 0;
  for (int i = 0; i < _peerCount; i++) {
    if (_peers[i].active) {
      count++;
    }
  }

  return count;
}

// Get the name of an available board by index
String PinComm::getAvailableBoardName(int index) {
  if (!_isConnected || index < 0) return "";

  int count = 0;
  for (int i = 0; i < _peerCount; i++) {
    if (_peers[i].active) {
      if (count == index) {
        return String(_peers[i].boardId);
      }
      count++;
    }
  }

  return "";
}

// Set a callback for when a new board is discovered
bool PinComm::onBoardDiscovered(DiscoveryCallback callback) {
  pinCommDebugLog("Setting board discovery callback");

  _discoveryCallback = callback;
  return true;
}

// Enable or disable message acknowledgements
bool PinComm::enableMessageAcknowledgements(bool enable) {
  _acknowledgementsEnabled = enable;

  if (enable) {
    pinCommDebugLog("Message acknowledgements enabled");
  } else {
    pinCommDebugLog("Message acknowledgements disabled");
  }

  return true;
}

// Check if message acknowledgements are enabled
bool PinComm::isAcknowledgementsEnabled() { return _acknowledgementsEnabled; }

// Enable or disable debug logging
bool PinComm::enableDebugLogging(bool enable) {
  _debugLoggingEnabled = enable;

  if (enable) {
    pinCommDebugLog("Debug logging enabled");
  } else {
    Serial.println("[PinComm] Debug logging disabled");
  }

  return true;
}

// Check if debug logging is enabled
bool PinComm::isDebugLoggingEnabled() { return _debugLoggingEnabled; }

// Enable or disable verbose logging
bool PinComm::enableVerboseLogging(bool enable) {
  _verboseLoggingEnabled = enable;

  if (enable) {
    pinCommDebugLog("Verbose logging enabled");
  } else if (_debugLoggingEnabled) {
    pinCommDebugLog("Verbose logging disabled");
  }

  return true;
}

// Check if verbose logging is enabled
bool PinComm::isVerboseLoggingEnabled() { return _verboseLoggingEnabled; }

// Register a callback for message send status
bool PinComm::onSendStatus(SendStatusCallback callback) {
  _sendStatusCallback = callback;
  return true;
}

// Register a callback specifically for message delivery failures
bool PinComm::onSendFailure(SendFailureCallback callback) {
  _sendFailureCallback = callback;
  return true;
}

// Generate a unique message ID
void PinComm::generateMessageId(char* buffer) {
  // Simple implementation: use timestamp + random number
  sprintf(buffer, "%lu-%u", millis(), random(0, 1000000));
}

// Board discovery
void PinComm::broadcastPresence() {
  if (!_isConnected) return;

  pinCommDebugLog("Broadcasting presence", _boardId);

  // Create a JSON document for the discovery message
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char messageId[37];
  generateMessageId(messageId);
  root["id"] = messageId;

  // Broadcast the discovery message
  broadcastMessage(MSG_TYPE_DISCOVERY, root);

  _lastDiscoveryBroadcast = millis();
}

// Add a peer to the list of known boards
bool PinComm::addPeer(const char* boardId) {
  if (boardId == NULL || strlen(boardId) == 0) return false;

  // Check if peer already exists
  for (int i = 0; i < _peerCount; i++) {
    if (strcmp(_peers[i].boardId, boardId) == 0) {
      // Update last seen time
      _peers[i].lastSeen = millis();
      _peers[i].active = true;
      return true;
    }
  }

  // Find an empty slot
  int emptySlot = -1;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!_peers[i].active) {
      emptySlot = i;
      break;
    }
  }

  // If no empty slot, check if we can reuse an old slot
  if (emptySlot == -1) {
    uint32_t oldestTime = millis();
    for (int i = 0; i < _peerCount; i++) {
      if (_peers[i].lastSeen < oldestTime) {
        oldestTime = _peers[i].lastSeen;
        emptySlot = i;
      }
    }
  }

  // If still no slot, we can't add more peers
  if (emptySlot == -1 || emptySlot >= MAX_PEERS) {
    pinCommDebugLog("Cannot add more peers, limit reached");
    return false;
  }

  // Add the new peer
  strncpy(_peers[emptySlot].boardId, boardId,
          sizeof(_peers[emptySlot].boardId) - 1);
  _peers[emptySlot].boardId[sizeof(_peers[emptySlot].boardId) - 1] = '\0';
  _peers[emptySlot].lastSeen = millis();
  _peers[emptySlot].active = true;

  // Update peer count if needed
  if (emptySlot >= _peerCount) {
    _peerCount = emptySlot + 1;
  }

  pinCommDebugLog("Added new peer", boardId);

  // Notify through callback if registered
  if (_discoveryCallback != NULL) {
    _discoveryCallback(boardId);
  }

  return true;
}

// Handle discovery messages
void PinComm::handleDiscovery(const char* senderId) {
  if (senderId == NULL || strlen(senderId) == 0) return;

  // Skip if it's our own message
  if (strcmp(senderId, _boardId) == 0) return;

  pinCommDebugLog("Received discovery from", senderId);

  // Add the sender as a peer
  addPeer(senderId);

  // Send a discovery response
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char messageId[37];
  generateMessageId(messageId);
  root["id"] = messageId;

  // Send the discovery response
  sendMessage(senderId, MSG_TYPE_DISCOVERY_RESPONSE, root);
}

// Process queued pin read responses
void PinComm::processQueuedResponses() {
  // Find matching pin read requests in tracked messages
  for (int i = 0; i < _queuedResponseCount; i++) {
    if (_queuedResponses[i].active) {
      // Find the corresponding request
      for (int j = 0; j < _trackedMessageCount; j++) {
        if (_trackedMessages[j].active &&
            _trackedMessages[j].messageType == MSG_TYPE_PIN_READ_REQUEST &&
            strcmp(_trackedMessages[j].targetBoard,
                   _queuedResponses[i].targetBoard) == 0 &&
            _trackedMessages[j].pin == _queuedResponses[i].pin) {
          // Found a matching request
          // Call the callback if registered
          PinReadResponseCallback callback =
              (PinReadResponseCallback)_trackedMessages[j].confirmCallback;
          if (callback != NULL) {
            callback(_queuedResponses[i].targetBoard, _queuedResponses[i].pin,
                     _queuedResponses[i].value, _queuedResponses[i].success);
          }

          // Mark both the request and response as inactive
          _trackedMessages[j].active = false;
          _queuedResponses[i].active = false;

          break;
        }
      }

      // If the response is still active, it might be for a synchronous read
      // or the request might have timed out
      // We'll keep it in the queue for a while
      if (_queuedResponses[i].active &&
          millis() - _queuedResponses[i].queuedTime > 5000) {
        // Too old, remove it
        _queuedResponses[i].active = false;
      }
    }
  }
}

void PinComm::processIncomingMessage(const uint8_t* data, size_t length) {
  if (data == NULL || length == 0) return;

  // Parse the JSON message
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, data, length);

  if (error) {
    pinCommDebugLog("JSON parsing error", error.c_str());
    return;
  }

  // Extract common fields
  const char* sender = doc["sender"];
  const char* messageId = doc["id"];
  uint8_t messageType = doc["type"];

  if (sender == NULL || messageId == NULL) {
    pinCommDebugLog("Invalid message: missing sender or ID");
    return;
  }

  // Skip our own messages
  if (strcmp(sender, _boardId) == 0) {
    return;
  }

  if (_verboseLoggingEnabled) {
    char buffer[100];
    sprintf(buffer, "Received message type %d from %s", messageType, sender);
    pinCommDebugLog(buffer);
  }

  // Add the sender as a peer if not already known
  addPeer(sender);

  // Send acknowledgement if enabled
  if (_acknowledgementsEnabled && messageType != MSG_TYPE_ACKNOWLEDGEMENT) {
    sendAcknowledgement(sender, messageId);
  }

  // Process based on message type
  switch (messageType) {
    case MSG_TYPE_PIN_CONTROL: {
      uint8_t pin = doc["pin"];
      uint8_t value = doc["value"];

      pinCommDebugLog("Received pin control", String(pin).c_str());

      // Check if we have a specific subscription for this pin from this sender
      bool handled = false;
      for (int i = 0; i < _subscriptionCount; i++) {
        if (_subscriptions[i].active &&
            _subscriptions[i].type == MSG_TYPE_PIN_CONTROL &&
            _subscriptions[i].pin == pin &&
            strcmp(_subscriptions[i].targetBoard, sender) == 0) {
          // Call the callback
          PinChangeCallback callback =
              (PinChangeCallback)_subscriptions[i].callback;
          if (callback != NULL) {
            callback(sender, pin, value);
            handled = true;
          }
        }
      }

      // If not handled by a specific subscription, use the global handler
      if (!handled && _pinControlConfirmCallback != NULL) {
        _pinControlConfirmCallback(sender, pin, value, true);
      }
      break;
    }

    case MSG_TYPE_PIN_READ_REQUEST: {
      uint8_t pin = doc["pin"];

      pinCommDebugLog("Received pin read request", String(pin).c_str());

      // Read the pin value
      uint8_t value = 0;
      bool success = false;

      if (_pinReadCallback != NULL) {
        // Use the custom callback
        value = _pinReadCallback(pin);
        success = true;
      } else {
        // Use digitalRead
        value = digitalRead(pin);
        success = true;
      }

      // Send the response
      StaticJsonDocument<200> responseDoc;
      JsonObject root = responseDoc.to<JsonObject>();

      // Add message ID and sender
      root["id"] = messageId;
      root["sender"] = _boardId;
      root["pin"] = pin;
      root["value"] = value;
      root["success"] = success;

      // Send the response
      sendMessage(sender, MSG_TYPE_PIN_READ_RESPONSE, root);
      break;
    }

    case MSG_TYPE_PIN_READ_RESPONSE: {
      uint8_t pin = doc["pin"];
      uint8_t value = doc["value"];
      bool success = doc["success"];

      pinCommDebugLog("Received pin read response", String(pin).c_str());

      // Queue the response for processing
      queuePinReadResponse(sender, pin, value, success, messageId);
      break;
    }

    case MSG_TYPE_ACKNOWLEDGEMENT: {
      const char* ackMessageId = doc["ack_id"];
      if (ackMessageId != NULL) {
        handleAcknowledgement(sender, ackMessageId);
      }
      break;
    }

    case MSG_TYPE_DISCOVERY: {
      handleDiscovery(sender);
      break;
    }

    case MSG_TYPE_DISCOVERY_RESPONSE: {
      // Already handled by adding the peer
      break;
    }

    case MSG_TYPE_PIN_PUBLISH: {
      uint8_t pin = doc["pin"];
      uint8_t value = doc["value"];

      // Check if we have subscriptions for this pin state
      for (int i = 0; i < _subscriptionCount; i++) {
        if (_subscriptions[i].active &&
            _subscriptions[i].type == MSG_TYPE_PIN_PUBLISH &&
            _subscriptions[i].pin == pin &&
            strcmp(_subscriptions[i].targetBoard, sender) == 0) {
          // Call the callback
          PinChangeCallback callback =
              (PinChangeCallback)_subscriptions[i].callback;
          if (callback != NULL) {
            callback(sender, pin, value);
          }
        }
      }
      break;
    }

    case MSG_TYPE_MESSAGE: {
      const char* topic = doc["topic"];
      const char* message = doc["message"];

      if (topic != NULL && message != NULL) {
        // Check if we have subscriptions for this topic
        for (int i = 0; i < _subscriptionCount; i++) {
          if (_subscriptions[i].active &&
              _subscriptions[i].type == MSG_TYPE_MESSAGE &&
              strcmp(_subscriptions[i].topic, topic) == 0) {
            // Call the callback
            MessageCallback callback =
                (MessageCallback)_subscriptions[i].callback;
            if (callback != NULL) {
              callback(sender, topic, message);
            }
          }
        }
      }
      break;
    }

    case MSG_TYPE_DIRECT_MESSAGE: {
      const char* message = doc["message"];

      if (message != NULL && _directMessageCallback != NULL) {
        _directMessageCallback(sender, "", message);
      }
      break;
    }

    case MSG_TYPE_SERIAL_DATA: {
      const char* data = doc["data"];

      if (data != NULL && _serialDataCallback != NULL) {
        _serialDataCallback(sender, data);
      }
      break;
    }
  }
}

// Message sending helpers
bool PinComm::sendMessage(const char* targetBoard, uint8_t messageType,
                          const JsonObject& doc) {
  if (!_isConnected || targetBoard == NULL || strlen(targetBoard) == 0) {
    return false;
  }

  // Add message type to the JSON document
  doc["type"] = messageType;

  // Serialize the JSON document
  char buffer[MAX_PIN_DATA_SIZE];
  size_t len = serializeJson(doc, buffer, sizeof(buffer));

  if (len == 0 || len >= MAX_PIN_DATA_SIZE) {
    pinCommDebugLog("Message too large or serialization failed");
    return false;
  }

  if (_verboseLoggingEnabled) {
    char logBuffer[100];
    sprintf(logBuffer, "Sending message type %d to %s", messageType,
            targetBoard);
    pinCommDebugLog(logBuffer);
  }

  // Track the message for acknowledgement if enabled
  if (_acknowledgementsEnabled && messageType != MSG_TYPE_ACKNOWLEDGEMENT) {
    // Find an empty slot
    int slot = -1;
    for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
      if (!_trackedMessages[i].active) {
        slot = i;
        break;
      }
    }

    // If no empty slot, reuse the oldest one
    if (slot == -1) {
      uint32_t oldestTime = millis();
      for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
        if (_trackedMessages[i].sentTime < oldestTime) {
          oldestTime = _trackedMessages[i].sentTime;
          slot = i;
        }
      }
    }

    // If we found a slot, track the message
    if (slot >= 0 && slot < MAX_TRACKED_MESSAGES) {
      const char* messageId = doc["id"];
      if (messageId != NULL) {
        strncpy(_trackedMessages[slot].messageId, messageId,
                sizeof(_trackedMessages[slot].messageId) - 1);
        _trackedMessages[slot]
            .messageId[sizeof(_trackedMessages[slot].messageId) - 1] = '\0';

        strncpy(_trackedMessages[slot].targetBoard, targetBoard,
                sizeof(_trackedMessages[slot].targetBoard) - 1);
        _trackedMessages[slot]
            .targetBoard[sizeof(_trackedMessages[slot].targetBoard) - 1] = '\0';

        _trackedMessages[slot].acknowledged = false;
        _trackedMessages[slot].sentTime = millis();
        _trackedMessages[slot].active = true;
        _trackedMessages[slot].messageType = messageType;
        _trackedMessages[slot].retryCount = 0;
        _trackedMessages[slot].retryScheduled = false;

        // For pin control messages, store the pin and value
        if (messageType == MSG_TYPE_PIN_CONTROL) {
          _trackedMessages[slot].pin = doc["pin"];
          _trackedMessages[slot].value = doc["value"];

          // Store the callback if provided
          if (messageType == MSG_TYPE_PIN_CONTROL &&
              doc.containsKey("callback")) {
            _trackedMessages[slot].confirmCallback =
                (PinControlConfirmCallback)doc["callback"].as<uint32_t>();
          } else {
            _trackedMessages[slot].confirmCallback = NULL;
          }
        }

        if (_trackedMessageCount <= slot) {
          _trackedMessageCount = slot + 1;
        }
      }
    }
  }

  // Send the message
  return sendFrame((const uint8_t*)buffer, len);
}

bool PinComm::broadcastMessage(uint8_t messageType, const JsonObject& doc) {
  if (!_isConnected) return false;

  // Add message type to the JSON document
  doc["type"] = messageType;

  // Serialize the JSON document
  char buffer[MAX_PIN_DATA_SIZE];
  size_t len = serializeJson(doc, buffer, sizeof(buffer));

  if (len == 0 || len >= MAX_PIN_DATA_SIZE) {
    pinCommDebugLog("Message too large or serialization failed");
    return false;
  }

  if (_verboseLoggingEnabled) {
    char logBuffer[100];
    sprintf(logBuffer, "Broadcasting message type %d", messageType);
    pinCommDebugLog(logBuffer);
  }

  // Send the message
  return sendFrame((const uint8_t*)buffer, len);
}

void PinComm::sendAcknowledgement(const char* sender, const char* messageId) {
  if (!_isConnected || sender == NULL || messageId == NULL) return;

  pinCommDebugLog("Sending acknowledgement", messageId);

  // Create a JSON document for the acknowledgement
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char ackMessageId[37];
  generateMessageId(ackMessageId);
  root["id"] = ackMessageId;

  // Add the ID of the message being acknowledged
  root["ack_id"] = messageId;

  // Send the acknowledgement
  sendMessage(sender, MSG_TYPE_ACKNOWLEDGEMENT, root);
}

void PinComm::handleAcknowledgement(const char* sender, const char* messageId) {
  if (sender == NULL || messageId == NULL) return;

  if (_verboseLoggingEnabled) {
    pinCommDebugLog("Received acknowledgement", messageId);
  }

  // Find the message in the tracked messages
  for (int i = 0; i < _trackedMessageCount; i++) {
    if (_trackedMessages[i].active &&
        strcmp(_trackedMessages[i].messageId, messageId) == 0 &&
        strcmp(_trackedMessages[i].targetBoard, sender) == 0) {
      // Mark as acknowledged
      _trackedMessages[i].acknowledged = true;

      // For pin control messages with callbacks, notify of success
      if (_trackedMessages[i].messageType == MSG_TYPE_PIN_CONTROL &&
          _trackedMessages[i].confirmCallback != NULL) {
        _trackedMessages[i].confirmCallback(_trackedMessages[i].targetBoard,
                                            _trackedMessages[i].pin,
                                            _trackedMessages[i].value, true);
      }

      // Call send status callback if registered
      if (_sendStatusCallback != NULL) {
        _sendStatusCallback(_trackedMessages[i].targetBoard,
                            _trackedMessages[i].messageType, true);
      }

      // Mark as inactive
      _trackedMessages[i].active = false;

      break;
    }
  }
}

// Queue a pin read response
void PinComm::queuePinReadResponse(const char* targetBoard, uint8_t pin,
                                   uint8_t value, bool success,
                                   const char* messageId) {
  // Find a free slot in the queue
  int slot = -1;
  for (int i = 0; i < MAX_QUEUED_RESPONSES; i++) {
    if (!_queuedResponses[i].active) {
      slot = i;
      break;
    }
  }

  // If no free slot, reuse the oldest one
  if (slot == -1) {
    uint32_t oldestTime = millis();
    for (int i = 0; i < MAX_QUEUED_RESPONSES; i++) {
      if (_queuedResponses[i].queuedTime < oldestTime) {
        oldestTime = _queuedResponses[i].queuedTime;
        slot = i;
      }
    }
  }

  // If we found a slot, queue the response
  if (slot >= 0 && slot < MAX_QUEUED_RESPONSES) {
    strncpy(_queuedResponses[slot].targetBoard, targetBoard,
            sizeof(_queuedResponses[slot].targetBoard) - 1);
    _queuedResponses[slot]
        .targetBoard[sizeof(_queuedResponses[slot].targetBoard) - 1] = '\0';

    _queuedResponses[slot].pin = pin;
    _queuedResponses[slot].value = value;
    _queuedResponses[slot].success = success;

    strncpy(_queuedResponses[slot].messageId, messageId,
            sizeof(_queuedResponses[slot].messageId) - 1);
    _queuedResponses[slot]
        .messageId[sizeof(_queuedResponses[slot].messageId) - 1] = '\0';

    _queuedResponses[slot].active = true;
    _queuedResponses[slot].queuedTime = millis();

    if (_queuedResponseCount <= slot) {
      _queuedResponseCount = slot + 1;
    }
  }
}

// Control a pin on a remote board
bool PinComm::controlRemotePin(const char* targetBoardId, uint8_t pin,
                               uint8_t value,
                               PinControlConfirmCallback callback) {
  if (!_isConnected || targetBoardId == NULL || strlen(targetBoardId) == 0) {
    return false;
  }

  if (!isBoardAvailable(targetBoardId)) {
    pinCommDebugLog("Target board not available", targetBoardId);
    return false;
  }

  pinCommDebugLog("Controlling remote pin", String(pin).c_str());

  // Create a JSON document for the pin control message
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char messageId[37];
  generateMessageId(messageId);
  root["id"] = messageId;

  // Add pin control information
  root["pin"] = pin;
  root["value"] = value;

  // Store the callback for this specific message
  if (callback != NULL) {
    // We can't directly store the callback in the JSON document
    // Instead, we'll store it in the tracked message after sending
  }

  // Send the message
  bool result = sendMessage(targetBoardId, MSG_TYPE_PIN_CONTROL, root);

  // If the message was sent successfully and we have a callback,
  // find the tracked message and store the callback
  if (result && callback != NULL) {
    for (int i = 0; i < _trackedMessageCount; i++) {
      if (_trackedMessages[i].active &&
          strcmp(_trackedMessages[i].messageId, messageId) == 0 &&
          strcmp(_trackedMessages[i].targetBoard, targetBoardId) == 0) {
        _trackedMessages[i].confirmCallback = callback;
        break;
      }
    }
  }

  return result;
}

// Clear all pin control confirmation callbacks
bool PinComm::clearRemotePinConfirmCallback() {
  pinCommDebugLog("Clearing pin control confirmation callbacks");

  _pinControlConfirmCallback = NULL;

  // Also clear callbacks in tracked messages
  for (int i = 0; i < _trackedMessageCount; i++) {
    if (_trackedMessages[i].active &&
        _trackedMessages[i].messageType == MSG_TYPE_PIN_CONTROL) {
      _trackedMessages[i].confirmCallback = NULL;
    }
  }

  return true;
}

// Set up handling of pin control messages
bool PinComm::handlePinControl(PinChangeCallback callback) {
  if (!_isConnected) return false;

  if (callback == NULL) {
    pinCommDebugLog("Setting up automatic pin control handling");
    g_pinChangeCallback = NULL;
    _pinControlConfirmCallback = NULL;
  } else {
    pinCommDebugLog("Setting up pin control handling with callback");

    // Store the callback in the global variable
    g_pinChangeCallback = callback;

    // Set the wrapper function as the confirm callback
    _pinControlConfirmCallback = pinControlConfirmWrapper;
  }

  return true;
}

// Stop handling pin control messages
bool PinComm::stopHandlingPinControl() {
  pinCommDebugLog("Stopping pin control handling");

  // Clear both callbacks
  g_pinChangeCallback = NULL;
  _pinControlConfirmCallback = NULL;

  return true;
}

// Accept pin control from a specific board for a specific pin
bool PinComm::acceptPinControlFrom(const char* controllerBoardId, uint8_t pin,
                                   PinChangeCallback callback) {
  if (!_isConnected || controllerBoardId == NULL ||
      strlen(controllerBoardId) == 0 || callback == NULL) {
    return false;
  }

  pinCommDebugLog("Accepting pin control from", controllerBoardId);

  // Find an empty slot
  int emptySlot = -1;
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (!_subscriptions[i].active) {
      emptySlot = i;
      break;
    }
  }

  // If no empty slot, we can't add more subscriptions
  if (emptySlot == -1) {
    pinCommDebugLog("Cannot add more subscriptions, limit reached");
    return false;
  }

  // Add the subscription
  strncpy(_subscriptions[emptySlot].targetBoard, controllerBoardId,
          sizeof(_subscriptions[emptySlot].targetBoard) - 1);
  _subscriptions[emptySlot]
      .targetBoard[sizeof(_subscriptions[emptySlot].targetBoard) - 1] = '\0';

  _subscriptions[emptySlot].pin = pin;
  _subscriptions[emptySlot].type = MSG_TYPE_PIN_CONTROL;
  _subscriptions[emptySlot].callback = (void*)callback;
  _subscriptions[emptySlot].active = true;

  // Update subscription count if needed
  if (_subscriptionCount <= emptySlot) {
    _subscriptionCount = emptySlot + 1;
  }

  return true;
}

// Stop accepting pin control from a specific board for a specific pin
bool PinComm::stopAcceptingPinControlFrom(const char* controllerBoardId,
                                          uint8_t pin) {
  if (!_isConnected || controllerBoardId == NULL ||
      strlen(controllerBoardId) == 0) {
    return false;
  }

  pinCommDebugLog("Stopping pin control from", controllerBoardId);

  // Find the subscription
  for (int i = 0; i < _subscriptionCount; i++) {
    if (_subscriptions[i].active &&
        _subscriptions[i].type == MSG_TYPE_PIN_CONTROL &&
        _subscriptions[i].pin == pin &&
        strcmp(_subscriptions[i].targetBoard, controllerBoardId) == 0) {
      // Remove the subscription
      _subscriptions[i].active = false;
      return true;
    }
  }

  return false;
}

// Handle pin read requests from other boards
bool PinComm::handlePinReadRequests(uint8_t (*pinReadCallback)(uint8_t pin)) {
  if (!_isConnected) return false;

  if (pinReadCallback == NULL) {
    pinCommDebugLog("Setting up automatic pin reading (using digitalRead)");
  } else {
    pinCommDebugLog("Setting up pin reading with custom callback");
  }

  // Store the callback
  _pinReadCallback = pinReadCallback;

  return true;
}

// Read the value of a pin on a remote board
bool PinComm::readRemotePin(const char* targetBoardId, uint8_t pin,
                            PinReadResponseCallback callback) {
  if (!_isConnected || targetBoardId == NULL || strlen(targetBoardId) == 0 ||
      callback == NULL) {
    return false;
  }

  if (!isBoardAvailable(targetBoardId)) {
    pinCommDebugLog("Target board not available", targetBoardId);
    return false;
  }

  pinCommDebugLog("Reading remote pin", String(pin).c_str());

  // Create a JSON document for the pin read request
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char messageId[37];
  generateMessageId(messageId);
  root["id"] = messageId;

  // Add pin information
  root["pin"] = pin;

  // Send the message
  bool result = sendMessage(targetBoardId, MSG_TYPE_PIN_READ_REQUEST, root);

  // If the message was sent successfully, find the tracked message and store
  // the callback
  if (result) {
    for (int i = 0; i < _trackedMessageCount; i++) {
      if (_trackedMessages[i].active &&
          strcmp(_trackedMessages[i].messageId, messageId) == 0 &&
          strcmp(_trackedMessages[i].targetBoard, targetBoardId) == 0) {
        _trackedMessages[i].pin = pin;
        _trackedMessages[i].confirmCallback =
            (PinControlConfirmCallback)callback;
        break;
      }
    }
  }

  return result;
}

// Read the value of a pin on a remote board synchronously
uint8_t PinComm::readRemotePinSync(const char* targetBoardId, uint8_t pin) {
  if (!_isConnected || targetBoardId == NULL || strlen(targetBoardId) == 0) {
    return 0;
  }

  if (!isBoardAvailable(targetBoardId)) {
    pinCommDebugLog("Target board not available", targetBoardId);
    return 0;
  }

  pinCommDebugLog("Reading remote pin synchronously", String(pin).c_str());

  // Create a JSON document for the pin read request
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char messageId[37];
  generateMessageId(messageId);
  root["id"] = messageId;

  // Add pin information
  root["pin"] = pin;

  // Send the message
  bool result = sendMessage(targetBoardId, MSG_TYPE_PIN_READ_REQUEST, root);

  if (!result) {
    pinCommDebugLog("Failed to send pin read request");
    return 0;
  }

  // Wait for the response
  uint32_t startTime = millis();
  uint8_t value = 0;
  bool responseReceived = false;

  while (millis() - startTime < 5000) {  // 5 second timeout
    // Check for incoming messages
    receiveFrame();

    // Check if we have a response
    for (int i = 0; i < _queuedResponseCount; i++) {
      if (_queuedResponses[i].active &&
          strcmp(_queuedResponses[i].targetBoard, targetBoardId) == 0 &&
          _queuedResponses[i].pin == pin) {
        // Found a response
        value = _queuedResponses[i].value;
        responseReceived = true;

        // Mark as inactive
        _queuedResponses[i].active = false;

        break;
      }
    }

    if (responseReceived) {
      break;
    }

    // Small delay to avoid hogging the CPU
    delay(10);
  }

  if (!responseReceived) {
    pinCommDebugLog("Pin read request timed out");
  }

  return value;
}

// Broadcast the state of a pin to all boards
bool PinComm::broadcastPinState(uint8_t pin, uint8_t value) {
  if (!_isConnected) return false;

  pinCommDebugLog("Broadcasting pin state", String(pin).c_str());

  // Create a JSON document for the pin state broadcast
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char messageId[37];
  generateMessageId(messageId);
  root["id"] = messageId;

  // Add pin information
  root["pin"] = pin;
  root["value"] = value;

  // Broadcast the message
  return broadcastMessage(MSG_TYPE_PIN_PUBLISH, root);
}

// Listen for pin state broadcasts from a specific board for a specific pin
bool PinComm::listenForPinStateFrom(const char* broadcasterBoardId, uint8_t pin,
                                    PinChangeCallback callback) {
  if (!_isConnected || broadcasterBoardId == NULL ||
      strlen(broadcasterBoardId) == 0 || callback == NULL) {
    return false;
  }

  pinCommDebugLog("Listening for pin state from", broadcasterBoardId);

  // Find an empty slot
  int emptySlot = -1;
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (!_subscriptions[i].active) {
      emptySlot = i;
      break;
    }
  }

  // If no empty slot, we can't add more subscriptions
  if (emptySlot == -1) {
    pinCommDebugLog("Cannot add more subscriptions, limit reached");
    return false;
  }

  // Add the subscription
  strncpy(_subscriptions[emptySlot].targetBoard, broadcasterBoardId,
          sizeof(_subscriptions[emptySlot].targetBoard) - 1);
  _subscriptions[emptySlot]
      .targetBoard[sizeof(_subscriptions[emptySlot].targetBoard) - 1] = '\0';

  _subscriptions[emptySlot].pin = pin;
  _subscriptions[emptySlot].type = MSG_TYPE_PIN_PUBLISH;
  _subscriptions[emptySlot].callback = (void*)callback;
  _subscriptions[emptySlot].active = true;

  // Update subscription count if needed
  if (_subscriptionCount <= emptySlot) {
    _subscriptionCount = emptySlot + 1;
  }

  return true;
}

// Stop listening for pin state broadcasts from a specific board for a specific
// pin
bool PinComm::stopListeningForPinStateFrom(const char* broadcasterBoardId,
                                           uint8_t pin) {
  if (!_isConnected || broadcasterBoardId == NULL ||
      strlen(broadcasterBoardId) == 0) {
    return false;
  }

  pinCommDebugLog("Stopping listening for pin state from", broadcasterBoardId);

  // Find the subscription
  for (int i = 0; i < _subscriptionCount; i++) {
    if (_subscriptions[i].active &&
        _subscriptions[i].type == MSG_TYPE_PIN_PUBLISH &&
        _subscriptions[i].pin == pin &&
        strcmp(_subscriptions[i].targetBoard, broadcasterBoardId) == 0) {
      // Remove the subscription
      _subscriptions[i].active = false;
      return true;
    }
  }

  return false;
}

// Publish a message to a topic that all boards can subscribe to
bool PinComm::publishTopic(const char* topic, const char* message) {
  if (!_isConnected || topic == NULL || strlen(topic) == 0 || message == NULL) {
    return false;
  }

  pinCommDebugLog("Publishing to topic", topic);

  // Create a JSON document for the topic message
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char messageId[37];
  generateMessageId(messageId);
  root["id"] = messageId;

  // Add topic and message
  root["topic"] = topic;
  root["message"] = message;

  // Broadcast the message
  return broadcastMessage(MSG_TYPE_MESSAGE, root);
}

// Subscribe to a topic to receive messages
bool PinComm::subscribeTopic(const char* topic, MessageCallback callback) {
  if (!_isConnected || topic == NULL || strlen(topic) == 0 ||
      callback == NULL) {
    return false;
  }

  pinCommDebugLog("Subscribing to topic", topic);

  // Find an empty slot
  int emptySlot = -1;
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (!_subscriptions[i].active) {
      emptySlot = i;
      break;
    }
  }

  // If no empty slot, we can't add more subscriptions
  if (emptySlot == -1) {
    pinCommDebugLog("Cannot add more subscriptions, limit reached");
    return false;
  }

  // Add the subscription
  strncpy(_subscriptions[emptySlot].topic, topic,
          sizeof(_subscriptions[emptySlot].topic) - 1);
  _subscriptions[emptySlot].topic[sizeof(_subscriptions[emptySlot].topic) - 1] =
      '\0';

  _subscriptions[emptySlot].type = MSG_TYPE_MESSAGE;
  _subscriptions[emptySlot].callback = (void*)callback;
  _subscriptions[emptySlot].active = true;

  // Update subscription count if needed
  if (_subscriptionCount <= emptySlot) {
    _subscriptionCount = emptySlot + 1;
  }

  return true;
}

// Unsubscribe from a topic
bool PinComm::unsubscribeTopic(const char* topic) {
  if (!_isConnected || topic == NULL || strlen(topic) == 0) {
    return false;
  }

  pinCommDebugLog("Unsubscribing from topic", topic);

  // Find the subscription
  for (int i = 0; i < _subscriptionCount; i++) {
    if (_subscriptions[i].active &&
        _subscriptions[i].type == MSG_TYPE_MESSAGE &&
        strcmp(_subscriptions[i].topic, topic) == 0) {
      // Remove the subscription
      _subscriptions[i].active = false;
      return true;
    }
  }

  return false;
}

// Forward serial data to all boards
bool PinComm::forwardSerialData(const char* data) {
  if (!_isConnected || data == NULL) {
    return false;
  }

  pinCommDebugLog("Forwarding serial data");

  // Create a JSON document for the serial data
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char messageId[37];
  generateMessageId(messageId);
  root["id"] = messageId;

  // Add the data
  root["data"] = data;

  // Broadcast the message
  return broadcastMessage(MSG_TYPE_SERIAL_DATA, root);
}

// Receive serial data from other boards
bool PinComm::receiveSerialData(SerialDataCallback callback) {
  if (!_isConnected || callback == NULL) {
    return false;
  }

  pinCommDebugLog("Setting up serial data reception");

  // Store the callback
  _serialDataCallback = callback;

  return true;
}

// Stop receiving serial data
bool PinComm::stopReceivingSerialData() {
  pinCommDebugLog("Stopping serial data reception");

  // Clear the callback
  _serialDataCallback = NULL;

  return true;
}

// Send a direct message to a specific board
bool PinComm::sendMessageToBoardId(const char* targetBoardId,
                                   const char* message) {
  if (!_isConnected || targetBoardId == NULL || strlen(targetBoardId) == 0 ||
      message == NULL) {
    return false;
  }

  if (!isBoardAvailable(targetBoardId)) {
    pinCommDebugLog("Target board not available", targetBoardId);
    return false;
  }

  pinCommDebugLog("Sending direct message to", targetBoardId);

  // Create a JSON document for the direct message
  StaticJsonDocument<200> doc;
  JsonObject root = doc.to<JsonObject>();

  // Add sender information
  root["sender"] = _boardId;

  // Generate a message ID
  char messageId[37];
  generateMessageId(messageId);
  root["id"] = messageId;

  // Add the message
  root["message"] = message;

  // Send the message
  return sendMessage(targetBoardId, MSG_TYPE_DIRECT_MESSAGE, root);
}

// Receive direct messages from other boards
bool PinComm::receiveMessagesFromBoards(MessageCallback callback) {
  if (!_isConnected || callback == NULL) {
    return false;
  }

  pinCommDebugLog("Setting up direct message reception");

  // Store the callback
  _directMessageCallback = callback;

  return true;
}

// Enable or disable pin control retries
bool PinComm::enablePinControlRetries(bool enable) {
  _pinControlRetriesEnabled = enable;

  if (enable) {
    pinCommDebugLog("Pin control retries enabled");
  } else {
    pinCommDebugLog("Pin control retries disabled");
  }

  return true;
}

// Check if automatic retries are enabled
bool PinComm::isPinControlRetriesEnabled() { return _pinControlRetriesEnabled; }

// Configure the maximum number of retries for pin control messages
bool PinComm::setPinControlMaxRetries(uint8_t maxRetries) {
  if (maxRetries > 10) {
    maxRetries = 10;  // Limit to a reasonable value
  }

  _pinControlMaxRetries = maxRetries;

  char buffer[50];
  sprintf(buffer, "Pin control max retries set to %d", maxRetries);
  pinCommDebugLog(buffer);

  return true;
}

// Get the current maximum number of retries for pin control messages
uint8_t PinComm::getPinControlMaxRetries() { return _pinControlMaxRetries; }

// Configure the delay between retries for pin control messages
bool PinComm::setPinControlRetryDelay(uint16_t retryDelayMs) {
  if (retryDelayMs < 50) {
    retryDelayMs = 50;  // Minimum delay
  } else if (retryDelayMs > MAX_RETRY_DELAY) {
    retryDelayMs = MAX_RETRY_DELAY;  // Maximum delay
  }

  _pinControlRetryDelay = retryDelayMs;

  char buffer[50];
  sprintf(buffer, "Pin control retry delay set to %d ms", retryDelayMs);
  pinCommDebugLog(buffer);

  return true;
}

// Get the current delay between retries for pin control messages
uint16_t PinComm::getPinControlRetryDelay() { return _pinControlRetryDelay; }

// Stop handling pin read requests
bool PinComm::stopHandlingPinReadRequests() {
  pinCommDebugLog("Stopping pin read request handler");

  // Clear the callback
  _pinReadCallback = NULL;

  return true;
}