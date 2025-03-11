/**
 * NetworkComm.cpp - Library for communication between ESP32 boards
 * Created by Claude, 2023
 * Modified to use ESP-NOW for direct communication
 */

#include "NetworkComm.h"

// Static pointer to the current NetworkComm instance for callbacks
static NetworkComm* _instance = nullptr;

// Debug function for consistent debug output formatting
void debugLog(const char* event, const char* details = nullptr) {
  // Only log if debugging is enabled in the NetworkComm instance
  if (_instance && _instance->isDebugLoggingEnabled()) {
    Serial.print("[NetworkComm] ");
    Serial.print(event);
    if (details != nullptr) {
      Serial.print(": ");
      Serial.print(details);
    }
    Serial.println();
  }
}

// Discovery broadcast interval (ms)
#define INITIAL_DISCOVERY_INTERVAL 2000  // 2 seconds initially
#define ACTIVE_DISCOVERY_INTERVAL 10000  // 10 seconds during active discovery
#define STABLE_DISCOVERY_INTERVAL 30000  // 30 seconds after stable connection

// Constructor
NetworkComm::NetworkComm() {
  _isConnected = false;
  _subscriptionCount = 0;
  _peerCount = 0;
  _directMessageCallback = NULL;
  _serialDataCallback = NULL;
  _discoveryCallback = NULL;  // Initialize discovery callback
  _pinControlConfirmCallback =
      NULL;  // Initialize pin control confirmation callback
  _lastDiscoveryBroadcast = 0;
  _acknowledgementsEnabled = false;
  _trackedMessageCount = 0;
  _debugLoggingEnabled = false;  // Debug logging off by default

  // Initialize subscriptions
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    _subscriptions[i].active = false;
  }

  // Initialize peers
  for (int i = 0; i < MAX_PEERS; i++) {
    _peers[i].active = false;
  }

  // Initialize tracked messages
  for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
    _trackedMessages[i].active = false;
  }

  // Store global instance pointer for callbacks
  _instance = this;
}

// ESP-NOW data received callback - MUST be minimal since it runs in interrupt
// context
void IRAM_ATTR NetworkComm::onDataReceived(const uint8_t* mac,
                                           const uint8_t* data, int len) {
  // DO NOT DO ANY PROCESSING OR SERIAL LOGGING HERE
  // Just pass data to the main code and return immediately
  if (_instance) {
    _instance->processIncomingMessage(mac, data, len);
  }
}

// Initialize with WiFi
bool NetworkComm::begin(const char* ssid, const char* password,
                        const char* boardId) {
  // Store board ID
  strncpy(_boardId, boardId, sizeof(_boardId) - 1);
  _boardId[sizeof(_boardId) - 1] = '\0';

  // Always print a startup message, regardless of debug setting
  Serial.print("[NetworkComm] Initializing board: ");
  Serial.print(boardId);
  Serial.print(", Debug: ");
  Serial.print(_debugLoggingEnabled ? "ON" : "OFF");
  Serial.print(", Acks: ");
  Serial.println(_acknowledgementsEnabled ? "ON" : "OFF");

  char debugMsg[100];
  sprintf(debugMsg, "board ID: %s, SSID: %s", boardId, ssid);
  debugLog("Initializing NetworkComm", debugMsg);

  // Connect to WiFi - ESP-NOW needs WiFi in station mode
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection (with timeout)
  unsigned long startTime = millis();
  Serial.print("[NetworkComm] Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - startTime > 10000) {
      Serial.println();
      Serial.println("[NetworkComm] WiFi connection timeout");
      debugLog("WiFi connection timeout");
      return false;  // Connection timeout
    }
  }
  Serial.println();
  Serial.print("[NetworkComm] Connected to WiFi, IP: ");
  Serial.println(WiFi.localIP());

  debugLog("WiFi connected successfully");

  // Get the MAC address
  WiFi.macAddress(_macAddress);
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", _macAddress[0],
          _macAddress[1], _macAddress[2], _macAddress[3], _macAddress[4],
          _macAddress[5]);

  Serial.print("[NetworkComm] Board MAC address: ");
  Serial.println(macStr);
  debugLog("Board MAC address", macStr);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[NetworkComm] ESP-NOW initialization failed");
    debugLog("ESP-NOW initialization failed");
    return false;
  }

  Serial.println("[NetworkComm] ESP-NOW initialized successfully");
  debugLog("ESP-NOW initialized successfully");

  // Register callback for receiving data
  esp_now_register_recv_cb(onDataReceived);
  Serial.println("[NetworkComm] ESP-NOW receive callback registered");

  _isConnected = true;

  // Broadcast presence to discover other boards
  broadcastPresence();

  debugLog("NetworkComm initialization complete");

  return true;
}

// Main loop function - must be called in loop()
void NetworkComm::update() {
  if (!_isConnected) return;

  uint32_t currentTime = millis();

  // Broadcast presence with varying frequency based on connection age
  static bool firstMinute = true;
  static bool firstFiveMinutes = true;
  static uint32_t startTime = currentTime;

  uint32_t discoveryInterval =
      INITIAL_DISCOVERY_INTERVAL;  // 2 seconds initially

  // After the first minute, slow down to 10 seconds
  if (firstMinute && (currentTime - startTime > 60000)) {
    firstMinute = false;
    discoveryInterval = ACTIVE_DISCOVERY_INTERVAL;  // 10 seconds
  }

  // After five minutes, slow down further to 30 seconds
  if (firstFiveMinutes && (currentTime - startTime > 300000)) {
    firstFiveMinutes = false;
    discoveryInterval = STABLE_DISCOVERY_INTERVAL;  // 30 seconds
  }

  if (currentTime - _lastDiscoveryBroadcast > discoveryInterval) {
    broadcastPresence();
    _lastDiscoveryBroadcast = currentTime;
  }

  // Process message acknowledgements if enabled
  if (_acknowledgementsEnabled) {
    // Check for message timeouts
    for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
      if (_trackedMessages[i].active) {
        // Check if message timed out
        if (!_trackedMessages[i].acknowledged &&
            (currentTime - _trackedMessages[i].sentTime > ACK_TIMEOUT)) {
          char debugMsg[100];
          sprintf(debugMsg, "Message %s to %s timed out (no acknowledgement)",
                  _trackedMessages[i].messageId,
                  _trackedMessages[i].targetBoard);
          debugLog(debugMsg);

          // If this was a pin control confirmation message, call the callback
          // with failure
          if (_pinControlConfirmCallback &&
              _trackedMessages[i].messageType == MSG_TYPE_PIN_CONTROL_CONFIRM) {
            // Call the callback with failure
            _pinControlConfirmCallback(_trackedMessages[i].targetBoard, 0, 0,
                                       false);
          }

          // Clean up this slot
          _trackedMessages[i].active = false;
        }
        // Clean up acknowledged messages after some time
        else if (_trackedMessages[i].acknowledged &&
                 (currentTime - _trackedMessages[i].sentTime >
                  ACK_TIMEOUT * 2)) {
          _trackedMessages[i].active = false;
        }
      }
    }
  }
}

// Broadcast presence for discovery
void NetworkComm::broadcastPresence() {
  if (!_isConnected) return;

  if (isDebugLoggingEnabled()) {
    Serial.println("[NetworkComm] Broadcasting presence");
  }

  // Send directly with broadcast address for maximum compatibility
  uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Register broadcast address if not registered
  if (esp_now_is_peer_exist(broadcastMac) == false) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }

  // Create a minimal discovery message
  StaticJsonDocument<128> doc;
  doc["sender"] = _boardId;
  doc["type"] = MSG_TYPE_DISCOVERY;

  // Serialize to JSON
  String jsonStr;
  serializeJson(doc, jsonStr);

  // Send directly to broadcast address
  esp_now_send(broadcastMac, (uint8_t*)jsonStr.c_str(), jsonStr.length() + 1);
}

// Handle discovery message
void NetworkComm::handleDiscovery(const char* senderId,
                                  const uint8_t* senderMac) {
  // Minimal logging
  Serial.print("[NetworkComm] Discovery from: ");
  Serial.println(senderId);

  // Add sender to peer list with minimal logging
  bool added = addPeer(senderId, senderMac);

  // If we have a discovery callback registered, call it
  if (_discoveryCallback != NULL) {
    _discoveryCallback(senderId);
  }

  // Don't send a response here - let the automatic broadcasting handle
  // discovery
}

// Add a peer to our list
bool NetworkComm::addPeer(const char* boardId, const uint8_t* macAddress) {
  // Basic validation
  if (!boardId || !macAddress) {
    Serial.println("[NetworkComm] Error: Invalid peer data");
    return false;
  }

  // Check if peer already exists
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_peers[i].active && strcmp(_peers[i].boardId, boardId) == 0) {
      // Update existing peer's last seen time
      _peers[i].lastSeen = millis();
      return true;  // Peer already exists, no need to log
    }
  }

  // Minimal logging for new peer
  Serial.print("[NetworkComm] Adding peer: ");
  Serial.println(boardId);

  // Find a free slot
  int slot = -1;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (!_peers[i].active) {
      slot = i;
      break;
    }
  }

  // If no free slot, find oldest peer
  if (slot == -1) {
    uint32_t oldestTime = UINT32_MAX;
    for (int i = 0; i < MAX_PEERS; i++) {
      if (_peers[i].lastSeen < oldestTime) {
        oldestTime = _peers[i].lastSeen;
        slot = i;
      }
    }
  }

  // Add the peer
  strncpy(_peers[slot].boardId, boardId, sizeof(_peers[slot].boardId) - 1);
  _peers[slot].boardId[sizeof(_peers[slot].boardId) - 1] = '\0';
  memcpy(_peers[slot].macAddress, macAddress, 6);
  _peers[slot].active = true;
  _peers[slot].lastSeen = millis();
  if (_peerCount < MAX_PEERS) _peerCount++;

  // Register with ESP-NOW
  if (esp_now_is_peer_exist(macAddress) == false) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, macAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    esp_err_t result = esp_now_add_peer(&peerInfo);
    if (result != ESP_OK) {
      Serial.println("[NetworkComm] Failed to add ESP-NOW peer");
    }
  }

  return true;
}

// Get MAC address for a board ID
bool NetworkComm::getMacForBoardId(const char* boardId, uint8_t* macAddress) {
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_peers[i].active && strcmp(_peers[i].boardId, boardId) == 0) {
      memcpy(macAddress, _peers[i].macAddress, 6);
      return true;
    }
  }
  return false;
}

// Process incoming ESP-NOW messages
void NetworkComm::processIncomingMessage(const uint8_t* mac,
                                         const uint8_t* data, int len) {
  // Ensure the data is valid
  if (len <= 0 || len > MAX_ESP_NOW_DATA_SIZE || !data || !mac) return;

  // Minimal logging
  Serial.print("[NetworkComm] Received message, length: ");
  Serial.println(len);

  // Create a copy of the data with null-termination
  char* message = new char[len + 1];
  if (!message) {
    Serial.println("[NetworkComm] Error: Failed to allocate memory");
    return;  // Memory allocation failed
  }

  memcpy(message, data, len);
  message[len] = '\0';

  // Parse JSON with a fixed-size buffer to prevent stack issues
  StaticJsonDocument<512>
      doc;  // Use StaticJsonDocument instead of DynamicJsonDocument
  DeserializationError error = deserializeJson(doc, message);

  // Free the memory immediately after parsing
  delete[] message;
  message = NULL;  // Avoid dangling pointer

  if (error) {
    // JSON parsing error - don't crash
    Serial.print("[NetworkComm] JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  // Get the sender ID and message type
  const char* sender = doc["sender"];
  uint8_t msgType = doc["type"];

  // Minimal sender info logging
  if (sender) {
    Serial.print("[NetworkComm] From: ");
    Serial.print(sender);
    Serial.print(", type: ");
    Serial.println(msgType);
  }

  // Process message based on type
  switch (msgType) {
    case MSG_TYPE_DISCOVERY:
      if (sender) {
        handleDiscovery(sender, mac);
      }
      break;

    case MSG_TYPE_DISCOVERY_RESPONSE:
      if (sender) {
        addPeer(sender, mac);
      }
      break;

    case MSG_TYPE_ACKNOWLEDGEMENT:
      // Handle acknowledgement
      if (doc.containsKey("messageId")) {
        const char* messageId = doc["messageId"];
        if (messageId && sender) {
          handleAcknowledgement(sender, messageId);
        }
      }
      break;

    case MSG_TYPE_PIN_CONTROL_CONFIRM:
      if (sender && doc.containsKey("pin") && doc.containsKey("value") &&
          doc.containsKey("messageId")) {
        uint8_t pin = doc["pin"];
        uint8_t value = doc["value"];
        const char* messageId = doc["messageId"];

        // Debug logging
        if (_debugLoggingEnabled) {
          char debugMsg[100];
          sprintf(debugMsg,
                  "Received pin control confirmation request from %s: pin=%d, "
                  "value=%d",
                  sender, pin, value);
          debugLog(debugMsg);
        }

        // Apply the pin value
        bool success = false;
        if (pin < NUM_DIGITAL_PINS) {
          pinMode(pin, OUTPUT);
          digitalWrite(pin, value);
          success = true;
        }

        // Send confirmation response
        DynamicJsonDocument responseDoc(128);
        responseDoc["pin"] = pin;
        responseDoc["value"] = value;
        responseDoc["success"] = success;
        responseDoc["messageId"] = messageId;

        sendMessage(sender, MSG_TYPE_PIN_CONTROL_RESPONSE,
                    responseDoc.as<JsonObject>());
      }
      break;

    case MSG_TYPE_PIN_CONTROL_RESPONSE:
      if (sender && doc.containsKey("pin") && doc.containsKey("value") &&
          doc.containsKey("success") && doc.containsKey("messageId")) {
        uint8_t pin = doc["pin"];
        uint8_t value = doc["value"];
        bool success = doc["success"];
        const char* messageId = doc["messageId"];

        // Debug logging
        if (_debugLoggingEnabled) {
          char debugMsg[100];
          sprintf(debugMsg,
                  "Received pin control confirmation response from %s: pin=%d, "
                  "value=%d, success=%d",
                  sender, pin, value, success);
          debugLog(debugMsg);
        }

        // Find the tracked message
        for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
          if (_trackedMessages[i].active &&
              strcmp(_trackedMessages[i].messageId, messageId) == 0) {
            // Mark as acknowledged
            _trackedMessages[i].acknowledged = true;

            // Call the callback if set
            if (_pinControlConfirmCallback) {
              _pinControlConfirmCallback(sender, pin, value, success);
            }

            // Clear the tracked message
            _trackedMessages[i].active = false;
            break;
          }
        }
      }
      break;

    case MSG_TYPE_PIN_CONTROL:
    case MSG_TYPE_PIN_PUBLISH:
      if (sender && doc.containsKey("pin") && doc.containsKey("value")) {
        uint8_t pin = doc["pin"];
        uint8_t value = doc["value"];

        // Debug logging for pin control/publish messages
        if (_debugLoggingEnabled) {
          char debugMsg[100];
          sprintf(debugMsg, "Received pin %s from %s: pin=%d, value=%d",
                  (msgType == MSG_TYPE_PIN_CONTROL) ? "control" : "publish",
                  sender, pin, value);
          debugLog(debugMsg);
        }

        // Process pin subscriptions
        for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
          if (_subscriptions[i].active &&
              _subscriptions[i].type == MSG_TYPE_PIN_SUBSCRIBE &&
              strcmp(_subscriptions[i].targetBoard, sender) == 0 &&
              _subscriptions[i].pin == pin) {
            PinChangeCallback callback =
                (PinChangeCallback)_subscriptions[i].callback;
            if (callback) {
              if (_debugLoggingEnabled) {
                debugLog("Executing pin change callback");
              }
              callback(sender, pin, value);
            }
          }
        }
      } else {
        if (_debugLoggingEnabled) {
          debugLog(
              "Received malformed pin message (missing sender, pin, or value)");
        }
      }
      break;

    case MSG_TYPE_MESSAGE:
      if (sender && doc.containsKey("topic") && doc.containsKey("message")) {
        const char* topic = doc["topic"];
        const char* msgContent = doc["message"];

        if (topic && msgContent) {
          // Process topic subscriptions
          for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
            if (_subscriptions[i].active &&
                _subscriptions[i].type == MSG_TYPE_MESSAGE &&
                strcmp(_subscriptions[i].topic, topic) == 0) {
              MessageCallback callback =
                  (MessageCallback)_subscriptions[i].callback;
              if (callback) {
                callback(sender, topic, msgContent);
              }
            }
          }
        }
      }
      break;

    case MSG_TYPE_SERIAL_DATA:
      if (sender && doc.containsKey("data")) {
        const char* data = doc["data"];
        if (data && _serialDataCallback) {
          _serialDataCallback(sender, data);
        }
      }
      break;

    case MSG_TYPE_DIRECT_MESSAGE:
      if (sender && doc.containsKey("message")) {
        const char* msgContent = doc["message"];
        if (msgContent && _directMessageCallback) {
          _directMessageCallback(sender, NULL, msgContent);
        }
      }
      break;
  }
}

// Send a message to a specific board
bool NetworkComm::sendMessage(const char* targetBoard, uint8_t messageType,
                              const JsonObject& doc) {
  if (!_isConnected) return false;
  if (!targetBoard) return false;

  // Get MAC address for target board
  uint8_t targetMac[6];
  if (!getMacForBoardId(targetBoard, targetMac)) {
    Serial.print("[NetworkComm] Unknown board: ");
    Serial.println(targetBoard);
    return false;  // Target board not found
  }

  // Create outgoing document
  StaticJsonDocument<384> outDoc;
  outDoc.set(doc);  // Copy contents from original doc

  // Add sender and type
  outDoc["sender"] = _boardId;
  outDoc["type"] = messageType;

  // Add message ID for tracking if acknowledgements are enabled
  char messageId[37] = {0};  // UUID string
  if (_acknowledgementsEnabled && messageType != MSG_TYPE_ACKNOWLEDGEMENT) {
    generateMessageId(messageId);
    outDoc["messageId"] = messageId;

    // Track this message for acknowledgement
    for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
      if (!_trackedMessages[i].active) {
        strncpy(_trackedMessages[i].messageId, messageId,
                sizeof(_trackedMessages[i].messageId) - 1);
        _trackedMessages[i]
            .messageId[sizeof(_trackedMessages[i].messageId) - 1] = '\0';

        strncpy(_trackedMessages[i].targetBoard, targetBoard,
                sizeof(_trackedMessages[i].targetBoard) - 1);
        _trackedMessages[i]
            .targetBoard[sizeof(_trackedMessages[i].targetBoard) - 1] = '\0';

        _trackedMessages[i].acknowledged = false;
        _trackedMessages[i].sentTime = millis();
        _trackedMessages[i].active = true;
        _trackedMessages[i].messageType = messageType;

        _trackedMessageCount++;
        break;
      }
    }
  }

  // Serialize to JSON
  String jsonStr;
  serializeJson(outDoc, jsonStr);

  // Check if message fits ESP-NOW size limit
  if (jsonStr.length() + 1 > MAX_ESP_NOW_DATA_SIZE) {
    Serial.println("[NetworkComm] Error: Message too large");
    return false;
  }

  // Send the message
  esp_err_t result =
      esp_now_send(targetMac, (uint8_t*)jsonStr.c_str(), jsonStr.length() + 1);
  return (result == ESP_OK);
}

// Broadcast message to all peers
bool NetworkComm::broadcastMessage(uint8_t messageType, const JsonObject& doc) {
  if (!_isConnected) return false;

  // Create outgoing document
  StaticJsonDocument<384> outDoc;
  outDoc.set(doc);  // Copy contents from original doc

  // Add sender and type
  outDoc["sender"] = _boardId;
  outDoc["type"] = messageType;

  // Serialize the JSON document
  String jsonStr;
  serializeJson(outDoc, jsonStr);

  // Check if message fits ESP-NOW size limit
  if (jsonStr.length() + 1 > MAX_ESP_NOW_DATA_SIZE) {
    Serial.println("[NetworkComm] Error: Message too large");
    return false;
  }

  // Always try the broadcast address first
  uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Register broadcast address if not already registered
  if (esp_now_is_peer_exist(broadcastMac) == false) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    esp_now_add_peer(&peerInfo);
  }

  // Send to broadcast address
  esp_err_t result = esp_now_send(broadcastMac, (uint8_t*)jsonStr.c_str(),
                                  jsonStr.length() + 1);
  return (result == ESP_OK);
}

// Check if connected to network
bool NetworkComm::isConnected() {
  return _isConnected && (WiFi.status() == WL_CONNECTED);
}

// Check if a specific board is available
bool NetworkComm::isBoardAvailable(const char* boardId) {
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_peers[i].active && strcmp(_peers[i].boardId, boardId) == 0) {
      return true;
    }
  }
  return false;
}

// Get count of available boards
int NetworkComm::getAvailableBoardsCount() { return _peerCount; }

// Get name of available board by index
String NetworkComm::getAvailableBoardName(int index) {
  int count = 0;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_peers[i].active) {
      if (count == index) {
        return String(_peers[i].boardId);
      }
      count++;
    }
  }
  return String("");
}

// Set pin value on remote board
bool NetworkComm::setPinValue(const char* targetBoard, uint8_t pin,
                              uint8_t value) {
  if (!_isConnected) return false;

  char debugMsg[100];
  sprintf(debugMsg, "board %s, pin %d, value %d", targetBoard, pin, value);
  debugLog("Setting pin value", debugMsg);

  DynamicJsonDocument doc(64);
  doc["pin"] = pin;
  doc["value"] = value;

  return sendMessage(targetBoard, MSG_TYPE_PIN_CONTROL, doc.as<JsonObject>());
}

// Set pin value on remote board with confirmation
bool NetworkComm::setPinValueWithConfirmation(
    const char* targetBoard, uint8_t pin, uint8_t value,
    PinControlConfirmCallback callback) {
  if (!_isConnected) return false;

  char debugMsg[100];
  sprintf(debugMsg, "board %s, pin %d, value %d (with confirmation)",
          targetBoard, pin, value);
  debugLog("Setting pin value", debugMsg);

  // Store the callback for later use
  _pinControlConfirmCallback = callback;

  DynamicJsonDocument doc(64);
  doc["pin"] = pin;
  doc["value"] = value;

  // Generate a message ID for tracking
  char messageId[37];
  generateMessageId(messageId);
  doc["messageId"] = messageId;

  // Track this message for confirmation
  for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
    if (!_trackedMessages[i].active) {
      strcpy(_trackedMessages[i].messageId, messageId);
      strcpy(_trackedMessages[i].targetBoard, targetBoard);
      _trackedMessages[i].acknowledged = false;
      _trackedMessages[i].sentTime = millis();
      _trackedMessages[i].active = true;
      break;
    }
  }

  return sendMessage(targetBoard, MSG_TYPE_PIN_CONTROL_CONFIRM,
                     doc.as<JsonObject>());
}

// Clear the pin control confirmation callback
bool NetworkComm::clearPinControlConfirmCallback() {
  debugLog("Clearing pin control confirmation callback");
  _pinControlConfirmCallback = NULL;
  return true;
}

// Get pin value from remote board (this would be async in reality)
uint8_t NetworkComm::getPinValue(const char* targetBoard, uint8_t pin) {
  // This would need to be implemented with a request/response pattern

  char debugMsg[100];
  sprintf(debugMsg, "board %s, pin %d", targetBoard, pin);
  debugLog("Getting pin value", debugMsg);

  // For simplicity, we're returning 0
  return 0;
}

// Subscribe to pin changes on remote board
bool NetworkComm::subscribeToPinChange(const char* targetBoard, uint8_t pin,
                                       PinChangeCallback callback) {
  if (!_isConnected) return false;

  char debugMsg[100];
  sprintf(debugMsg, "to board %s, pin %d", targetBoard, pin);
  debugLog("Subscribing to pin change", debugMsg);

  // Find free subscription slot
  int slot = -1;
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (!_subscriptions[i].active) {
      slot = i;
      break;
    }
  }

  if (slot == -1) return false;  // No free slots

  // Store subscription
  strncpy(_subscriptions[slot].targetBoard, targetBoard,
          sizeof(_subscriptions[slot].targetBoard) - 1);
  _subscriptions[slot]
      .targetBoard[sizeof(_subscriptions[slot].targetBoard) - 1] = '\0';
  _subscriptions[slot].pin = pin;
  _subscriptions[slot].type = MSG_TYPE_PIN_SUBSCRIBE;
  _subscriptions[slot].callback = (void*)callback;
  _subscriptions[slot].active = true;

  // Send subscription request to target board
  DynamicJsonDocument doc(64);
  doc["pin"] = pin;

  return sendMessage(targetBoard, MSG_TYPE_PIN_SUBSCRIBE, doc.as<JsonObject>());
}

// Unsubscribe from pin changes
bool NetworkComm::unsubscribeFromPinChange(const char* targetBoard,
                                           uint8_t pin) {
  if (!_isConnected) return false;

  char debugMsg[100];
  sprintf(debugMsg, "from board %s, pin %d", targetBoard, pin);
  debugLog("Unsubscribing from pin change", debugMsg);

  // Find matching subscription
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (_subscriptions[i].active &&
        _subscriptions[i].type == MSG_TYPE_PIN_SUBSCRIBE &&
        strcmp(_subscriptions[i].targetBoard, targetBoard) == 0 &&
        _subscriptions[i].pin == pin) {
      _subscriptions[i].active = false;
      return true;
    }
  }

  return false;
}

// Publish message to topic
bool NetworkComm::publish(const char* topic, const char* message) {
  if (!_isConnected) return false;

  char debugMsg[100];
  sprintf(debugMsg, "topic '%s', message: %s", topic, message);
  debugLog("Publishing message", debugMsg);

  DynamicJsonDocument doc(256);
  doc["topic"] = topic;
  doc["message"] = message;

  return broadcastMessage(MSG_TYPE_MESSAGE, doc.as<JsonObject>());
}

// Subscribe to topic
bool NetworkComm::subscribe(const char* topic, MessageCallback callback) {
  if (!_isConnected) return false;

  debugLog("Subscribing to topic", topic);

  // Find free subscription slot
  int slot = -1;
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (!_subscriptions[i].active) {
      slot = i;
      break;
    }
  }

  if (slot == -1) return false;  // No free slots

  // Store subscription
  strncpy(_subscriptions[slot].topic, topic,
          sizeof(_subscriptions[slot].topic) - 1);
  _subscriptions[slot].topic[sizeof(_subscriptions[slot].topic) - 1] = '\0';
  _subscriptions[slot].type = MSG_TYPE_MESSAGE;
  _subscriptions[slot].callback = (void*)callback;
  _subscriptions[slot].active = true;

  return true;
}

// Unsubscribe from topic
bool NetworkComm::unsubscribe(const char* topic) {
  if (!_isConnected) return false;

  debugLog("Unsubscribing from topic", topic);

  // Find matching subscription
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (_subscriptions[i].active &&
        _subscriptions[i].type == MSG_TYPE_MESSAGE &&
        strcmp(_subscriptions[i].topic, topic) == 0) {
      _subscriptions[i].active = false;
      return true;
    }
  }

  return false;
}

// Publish serial data
bool NetworkComm::publishSerialData(const char* data) {
  if (!_isConnected) return false;

  debugLog("Publishing serial data", data);

  DynamicJsonDocument doc(256);
  doc["data"] = data;

  return broadcastMessage(MSG_TYPE_SERIAL_DATA, doc.as<JsonObject>());
}

// Subscribe to serial data
bool NetworkComm::subscribeToSerialData(SerialDataCallback callback) {
  if (!_isConnected) return false;

  debugLog("Subscribing to serial data");

  _serialDataCallback = callback;
  return true;
}

// Unsubscribe from serial data
bool NetworkComm::unsubscribeFromSerialData() {
  if (!_isConnected) return false;

  debugLog("Unsubscribing from serial data");

  _serialDataCallback = NULL;
  return true;
}

// Send direct message to specific board
bool NetworkComm::sendDirectMessage(const char* targetBoard,
                                    const char* message) {
  if (!_isConnected) return false;

  char debugMsg[100];
  sprintf(debugMsg, "to %s: %s", targetBoard, message);
  debugLog("Sending direct message", debugMsg);

  DynamicJsonDocument doc(256);
  doc["message"] = message;

  return sendMessage(targetBoard, MSG_TYPE_DIRECT_MESSAGE,
                     doc.as<JsonObject>());
}

// Set callback for direct messages
bool NetworkComm::setDirectMessageCallback(MessageCallback callback) {
  debugLog("Setting direct message callback");

  _directMessageCallback = callback;
  return true;
}

// Enable or disable message acknowledgements
bool NetworkComm::enableMessageAcknowledgements(bool enable) {
  _acknowledgementsEnabled = enable;
  char debugMsg[50];
  sprintf(debugMsg, "Acknowledgements %s", enable ? "enabled" : "disabled");
  debugLog(debugMsg);
  return true;
}

// Check if acknowledgements are enabled
bool NetworkComm::isAcknowledgementsEnabled() {
  return _acknowledgementsEnabled;
}

// Enable or disable debug logging
bool NetworkComm::enableDebugLogging(bool enable) {
  _debugLoggingEnabled = enable;

  // This log will only appear if debug is being enabled
  if (enable) {
    Serial.println("[NetworkComm] Debug logging enabled");
  }
  return true;
}

// Check if debug logging is enabled
bool NetworkComm::isDebugLoggingEnabled() { return _debugLoggingEnabled; }

// Generate a simple UUID-like message ID
void NetworkComm::generateMessageId(char* buffer) {
  const char* chars = "0123456789abcdef";

  // Format: 8-4-4-4-12 (standard UUID format)
  int pos = 0;
  for (int i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      buffer[i] = '-';
    } else {
      uint8_t random_val = random(0, 16);
      buffer[i] = chars[random_val];
    }
  }
  buffer[36] = '\0';
}

// Send an acknowledgement for a received message
void NetworkComm::sendAcknowledgement(const char* sender,
                                      const char* messageId) {
  if (!_isConnected) return;

  DynamicJsonDocument doc(128);
  doc["messageId"] = messageId;

  char debugMsg[100];
  sprintf(debugMsg, "Acknowledging message %s to %s", messageId, sender);
  debugLog(debugMsg);

  sendMessage(sender, MSG_TYPE_ACKNOWLEDGEMENT, doc.as<JsonObject>());
}

// Handle an incoming acknowledgement message
void NetworkComm::handleAcknowledgement(const char* sender,
                                        const char* messageId) {
  char debugMsg[100];
  sprintf(debugMsg, "Received acknowledgement for %s from %s", messageId,
          sender);
  debugLog(debugMsg);

  // Find and update the tracked message
  for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
    if (_trackedMessages[i].active &&
        strcmp(_trackedMessages[i].messageId, messageId) == 0) {
      _trackedMessages[i].acknowledged = true;
      sprintf(debugMsg, "Message %s acknowledged by %s", messageId, sender);
      debugLog(debugMsg);
      break;
    }
  }
}

// Set callback for board discovery
bool NetworkComm::setDiscoveryCallback(DiscoveryCallback callback) {
  debugLog("Setting discovery callback");

  _discoveryCallback = callback;
  return true;
}
