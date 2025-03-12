/**
 * NetworkCore.cpp - Base class for ESP32 network communication
 * Created as part of the NetworkComm library refactoring
 */

#include "NetworkCore.h"

#include "NetworkDiscovery.h"
#include "NetworkPinControl.h"

// Static instance pointer for callbacks
NetworkCore* NetworkCore::_instance = nullptr;

// Constructor
NetworkCore::NetworkCore() {
  _isConnected = false;
  _peerCount = 0;
  _acknowledgementsEnabled = true;  // Enable acknowledgements by default
  _debugLoggingEnabled = false;     // Debug logging off by default
  _verboseLoggingEnabled = false;   // Verbose logging off by default
  _trackedMessageCount = 0;
  _sendStatusCallback = NULL;
  _sendFailureCallback = NULL;
  _discoveryHandler = NULL;  // Initialize discovery handler to NULL

  // Initialize peers
  for (int i = 0; i < MAX_PEERS; i++) {
    _peers[i].active = false;
  }

  // Initialize tracked messages
  for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
    _trackedMessages[i].active = false;
    _trackedMessages[i].confirmCallback = NULL;
    _trackedMessages[i].pin = 0;
    _trackedMessages[i].value = 0;
  }

  // Store global instance pointer for callbacks
  _instance = this;
}

NetworkCore::~NetworkCore() {
  // Clean up ESP-NOW
  esp_now_deinit();
}

// Initialize with WiFi
bool NetworkCore::begin(const char* ssid, const char* password,
                        const char* boardId) {
  // Store board ID
  strncpy(_boardId, boardId, sizeof(_boardId) - 1);
  _boardId[sizeof(_boardId) - 1] = '\0';

  // Always print a startup message
  Serial.print("[NetworkCore] Initializing board: ");
  Serial.print(boardId);
  Serial.print(", Debug: ");
  Serial.print(_debugLoggingEnabled ? "ON" : "OFF");
  Serial.print(", Acks: ");
  Serial.println(_acknowledgementsEnabled ? "ON" : "OFF");

  char debugMsg[100];
  sprintf(debugMsg, "board ID: %s, SSID: %s", boardId, ssid);
  debugLog("Initializing NetworkCore", debugMsg);

  // Connect to WiFi - ESP-NOW needs WiFi in station mode
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Wait for connection (with timeout)
  unsigned long startTime = millis();
  Serial.print("[NetworkCore] Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (millis() - startTime > 10000) {
      Serial.println();
      Serial.println("[NetworkCore] WiFi connection timeout");
      debugLog("WiFi connection timeout");
      return false;  // Connection timeout
    }
  }
  Serial.println();
  Serial.print("[NetworkCore] Connected to WiFi, IP: ");
  Serial.println(WiFi.localIP());

  debugLog("WiFi connected successfully");

  // Get the MAC address
  WiFi.macAddress(_macAddress);
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", _macAddress[0],
          _macAddress[1], _macAddress[2], _macAddress[3], _macAddress[4],
          _macAddress[5]);

  Serial.print("[NetworkCore] Board MAC address: ");
  Serial.println(macStr);
  debugLog("Board MAC address", macStr);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("[NetworkCore] ESP-NOW initialization failed");
    debugLog("ESP-NOW initialization failed");
    return false;
  }

  Serial.println("[NetworkCore] ESP-NOW initialized successfully");
  debugLog("ESP-NOW initialized successfully");

  // Register callback for receiving data
  esp_err_t recv_result = esp_now_register_recv_cb(onDataReceived);
  if (recv_result != ESP_OK) {
    Serial.print(
        "[NetworkCore] ESP-NOW receive callback registration failed with "
        "error: ");
    Serial.println(recv_result);
  } else {
    Serial.println("[NetworkCore] ESP-NOW receive callback registered");
  }

  // Register callback for send status
  esp_err_t send_result = esp_now_register_send_cb(onDataSent);
  if (send_result != ESP_OK) {
    Serial.print(
        "[NetworkCore] ESP-NOW send callback registration failed with error: ");
    Serial.println(send_result);
  } else {
    Serial.println("[NetworkCore] ESP-NOW send callback registered");
  }

  _isConnected = true;

  debugLog("NetworkCore initialization complete");

  return true;
}

// Main loop function - must be called regularly
void NetworkCore::update() {
  if (!_isConnected) return;

  uint32_t currentTime = millis();

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

          // Handle pin control callbacks separately
          if (_trackedMessages[i].messageType == MSG_TYPE_PIN_CONTROL &&
              _trackedMessages[i].confirmCallback != NULL) {
            // Call the callback with failure
            ((PinControlConfirmCallback)_trackedMessages[i].confirmCallback)(
                _trackedMessages[i].targetBoard, _trackedMessages[i].pin,
                _trackedMessages[i].value, false);
          }

          // Clean up this slot
          _trackedMessages[i].active = false;
          _trackedMessages[i].confirmCallback = NULL;
        }
        // Clean up acknowledged messages after some time
        else if (_trackedMessages[i].acknowledged &&
                 (currentTime - _trackedMessages[i].sentTime >
                  ACK_TIMEOUT * 2)) {
          _trackedMessages[i].active = false;
          _trackedMessages[i].confirmCallback = NULL;
        }
      }
    }
  }
}

bool NetworkCore::isConnected() {
  return _isConnected && (WiFi.status() == WL_CONNECTED);
}

bool NetworkCore::enableMessageAcknowledgements(bool enable) {
  _acknowledgementsEnabled = enable;
  char debugMsg[50];
  sprintf(debugMsg, "Acknowledgements %s", enable ? "enabled" : "disabled");
  debugLog(debugMsg);
  return true;
}

bool NetworkCore::isAcknowledgementsEnabled() {
  return _acknowledgementsEnabled;
}

bool NetworkCore::onSendStatus(SendStatusCallback callback) {
  _sendStatusCallback = callback;
  return true;
}

bool NetworkCore::onSendFailure(SendFailureCallback callback) {
  _sendFailureCallback = callback;
  return true;
}

// ESP-NOW callbacks
void IRAM_ATTR NetworkCore::onDataSent(const uint8_t* mac_addr,
                                       esp_now_send_status_t status) {
  // Just pass data to the main instance - don't do processing here (it's an
  // interrupt context)
  if (_instance) {
    _instance->handleSendStatus(mac_addr, status);
  }
}

void IRAM_ATTR NetworkCore::onDataReceived(const uint8_t* mac,
                                           const uint8_t* data, int len) {
  // Just pass data to the main instance - don't do processing here (it's an
  // interrupt context)
  if (_instance) {
    _instance->processIncomingMessage(mac, data, len);
  }
}

// Process ESP-NOW send status callback
void NetworkCore::handleSendStatus(const uint8_t* mac_addr,
                                   esp_now_send_status_t status) {
  // Find the board ID for this MAC address
  char targetBoardId[32] = {0};
  uint8_t messageType = 0;
  uint8_t pin = 0;
  uint8_t value = 0;
  bool boardFound = getBoardIdForMac(mac_addr, targetBoardId);
  bool isSuccess = (status == ESP_NOW_SEND_SUCCESS);
  bool isFailure = !isSuccess;

  // Check for tracked messages to this MAC address
  for (int i = 0; i < MAX_TRACKED_MESSAGES; i++) {
    if (_trackedMessages[i].active) {
      // Get MAC address for the target board
      uint8_t targetMac[6];
      if (getMacForBoardId(_trackedMessages[i].targetBoard, targetMac)) {
        // Compare MAC addresses
        bool macMatch = true;
        for (int j = 0; j < 6; j++) {
          if (targetMac[j] != mac_addr[j]) {
            macMatch = false;
            break;
          }
        }

        if (macMatch) {
          // Found a matching message
          messageType = _trackedMessages[i].messageType;
          pin = _trackedMessages[i].pin;
          value = _trackedMessages[i].value;

          // If this is a pin control message with a callback, call it
          if (_trackedMessages[i].confirmCallback != NULL &&
              _trackedMessages[i].messageType == MSG_TYPE_PIN_CONTROL) {
            ((PinControlConfirmCallback)_trackedMessages[i].confirmCallback)(
                _trackedMessages[i].targetBoard, _trackedMessages[i].pin,
                _trackedMessages[i].value, isSuccess);

            // Clean up the tracked message if it succeeded
            if (isSuccess) {
              _trackedMessages[i].active = false;
              _trackedMessages[i].confirmCallback = NULL;
            }
          }

          break;  // Found the message, no need to continue
        }
      }
    }
  }

  // Call the global send status callback if registered
  if (_sendStatusCallback != NULL && boardFound) {
    _sendStatusCallback(targetBoardId, messageType, isSuccess);
  }

  // Call the failure callback specifically if this was a failure
  if (isFailure && _sendFailureCallback != NULL && boardFound) {
    _sendFailureCallback(targetBoardId, messageType, pin, value);
  }
}

// Process incoming ESP-NOW messages
void NetworkCore::processIncomingMessage(const uint8_t* mac,
                                         const uint8_t* data, int len) {
  // Ensure the data is valid
  if (len <= 0 || len > MAX_ESP_NOW_DATA_SIZE || !data || !mac) return;

  // Minimal logging
  if (_verboseLoggingEnabled) {
    Serial.print("[NetworkCore] Received message, length: ");
    Serial.println(len);
  }

  // Create a copy of the data with null-termination
  char* message = new char[len + 1];
  if (!message) {
    Serial.println("[NetworkCore] Error: Failed to allocate memory");
    return;  // Memory allocation failed
  }

  memcpy(message, data, len);
  message[len] = '\0';

  // Parse JSON with a fixed-size buffer to prevent stack issues
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);

  // Free the memory immediately after parsing
  delete[] message;
  message = NULL;  // Avoid dangling pointer

  if (error) {
    // JSON parsing error - don't crash
    Serial.print("[NetworkCore] JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  // Get the sender ID and message type
  const char* sender = doc["sender"];
  uint8_t msgType = doc["type"];

  // Minimal sender info logging
  if (_verboseLoggingEnabled && sender) {
    Serial.print("[NetworkCore] From: ");
    Serial.print(sender);
    Serial.print(", type: ");
    Serial.println(msgType);
  }

  // Process message based on type
  switch (msgType) {
    case MSG_TYPE_DISCOVERY:
      // Discovery messages are handled by the NetworkDiscovery class
      Serial.print("[NETWORK] Received discovery message from: ");
      Serial.println(sender);

      // Forward to NetworkDiscovery class if handler is registered
      if (_discoveryHandler != NULL) {
        Serial.println("[NETWORK] Forwarding to discovery handler");
        _discoveryHandler->handleDiscovery(sender, mac);
      } else {
        Serial.println("[NETWORK] ERROR: No discovery handler registered");
      }
      break;

    case MSG_TYPE_DISCOVERY_RESPONSE:
      // Add sender to peer list
      if (sender) {
        Serial.print("[NETWORK] Received discovery response from: ");
        Serial.println(sender);

        // Format MAC address for debug output
        char macStr[18];
        sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
                mac[3], mac[4], mac[5]);
        Serial.print("[NETWORK] Response MAC: ");
        Serial.println(macStr);

        bool added = addPeer(sender, mac);
        Serial.print("[NETWORK] Peer added from response: ");
        Serial.println(added ? "YES" : "NO");
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

    case MSG_TYPE_PIN_CONTROL:
      // Pin control messages are handled by the NetworkPinControl class
      break;

    case MSG_TYPE_PIN_PUBLISH:
      // Pin publish messages are handled by the NetworkPinControl class
      break;

    case MSG_TYPE_MESSAGE:
      // Topic messages are handled by the NetworkMessaging class
      break;

    case MSG_TYPE_SERIAL_DATA:
      // Serial data messages are handled by the NetworkSerial class
      break;

    case MSG_TYPE_DIRECT_MESSAGE:
      // Direct messages are handled by the NetworkMessaging class
      break;
  }
}

// Helper method to send a message to a specific board
bool NetworkCore::sendMessage(const char* targetBoard, uint8_t messageType,
                              const JsonObject& doc) {
  if (!_isConnected) return false;
  if (!targetBoard) return false;

  // Get MAC address for target board
  uint8_t targetMac[6];
  if (!getMacForBoardId(targetBoard, targetMac)) {
    Serial.print("[NetworkCore] Unknown board: ");
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
    Serial.println("[NetworkCore] Error: Message too large");
    return false;
  }

  // Send the message
  esp_err_t result =
      esp_now_send(targetMac, (uint8_t*)jsonStr.c_str(), jsonStr.length() + 1);
  return (result == ESP_OK);
}

// Helper method to broadcast a message to all boards
bool NetworkCore::broadcastMessage(uint8_t messageType, const JsonObject& doc) {
  if (!_isConnected) {
    Serial.println("[NetworkCore] Cannot broadcast: not connected");
    return false;
  }

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
    Serial.println("[NetworkCore] Error: Message too large");
    return false;
  }

  // Use the broadcast address
  uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // Register broadcast address if not already registered
  if (esp_now_is_peer_exist(broadcastMac) == false) {
    Serial.println("[NetworkCore] Registering broadcast address as peer");
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    esp_err_t add_result = esp_now_add_peer(&peerInfo);
    if (add_result != ESP_OK) {
      Serial.print("[NetworkCore] Failed to add broadcast peer, error: ");
      Serial.println(add_result);
      return false;
    } else {
      Serial.println("[NetworkCore] Successfully registered broadcast address");
    }
  } else {
    Serial.println("[NetworkCore] Broadcast address already registered");
  }

  // Send to broadcast address
  Serial.print("[NetworkCore] Broadcasting message type ");
  Serial.print(messageType);
  Serial.print(", length: ");
  Serial.println(jsonStr.length());

  // For very verbose debugging, print the JSON
  if (_verboseLoggingEnabled) {
    Serial.print("[NetworkCore] Message content: ");
    Serial.println(jsonStr);
  }

  esp_err_t result = esp_now_send(broadcastMac, (uint8_t*)jsonStr.c_str(),
                                  jsonStr.length() + 1);
  if (result != ESP_OK) {
    Serial.print("[NetworkCore] Broadcast failed with error: ");
    Serial.println(result);
    return false;
  }

  Serial.println("[NetworkCore] Broadcast sent");
  return true;
}

// Helper method to get MAC address for a board ID
bool NetworkCore::getMacForBoardId(const char* boardId, uint8_t* macAddress) {
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_peers[i].active && strcmp(_peers[i].boardId, boardId) == 0) {
      memcpy(macAddress, _peers[i].macAddress, 6);
      return true;
    }
  }
  return false;
}

// Helper method to get board ID for a MAC address
bool NetworkCore::getBoardIdForMac(const uint8_t* macAddress, char* boardId) {
  if (!macAddress || !boardId) return false;

  // Check if it's broadcast address
  bool isBroadcast = true;
  for (int i = 0; i < 6; i++) {
    if (macAddress[i] != 0xFF) {
      isBroadcast = false;
      break;
    }
  }

  if (isBroadcast) {
    strcpy(boardId, "broadcast");
    return true;
  }

  // Check known peers
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_peers[i].active) {
      bool macMatch = true;
      for (int j = 0; j < 6; j++) {
        if (_peers[i].macAddress[j] != macAddress[j]) {
          macMatch = false;
          break;
        }
      }

      if (macMatch) {
        strcpy(boardId, _peers[i].boardId);
        return true;
      }
    }
  }

  return false;  // MAC address not found
}

// Add a peer to our list
bool NetworkCore::addPeer(const char* boardId, const uint8_t* macAddress) {
  // Basic validation
  if (!boardId || !macAddress) {
    Serial.println("[NetworkCore] Error: Invalid peer data");
    return false;
  }

  // Check if peer already exists
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_peers[i].active && strcmp(_peers[i].boardId, boardId) == 0) {
      // Update existing peer's last seen time
      _peers[i].lastSeen = millis();
      return true;  // Peer already exists
    }
  }

  // Minimal logging for new peer
  if (_debugLoggingEnabled) {
    Serial.print("[NetworkCore] Adding peer: ");
    Serial.println(boardId);
  }

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
      Serial.println("[NetworkCore] Failed to add ESP-NOW peer");
    }
  }

  return true;
}

// Send an acknowledgement for a received message
void NetworkCore::sendAcknowledgement(const char* sender,
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
void NetworkCore::handleAcknowledgement(const char* sender,
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
      break;
    }
  }
}

// Generate a simple UUID-like message ID
void NetworkCore::generateMessageId(char* buffer) {
  const char* chars = "0123456789abcdef";

  // Format: 8-4-4-4-12 (standard UUID format)
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

// Debug logging helper
void NetworkCore::debugLog(const char* event, const char* details) {
  if (_debugLoggingEnabled) {
    Serial.print("[NetworkCore] ");
    Serial.print(event);
    if (details != nullptr) {
      Serial.print(": ");
      Serial.print(details);
    }
    Serial.println();
  }
}

// Verbose logging helper
void NetworkCore::verboseLog(const char* event, const char* details) {
  if (_verboseLoggingEnabled) {
    Serial.print("[NetworkCore] [VERBOSE] ");
    Serial.print(event);
    if (details != nullptr) {
      Serial.print(": ");
      Serial.print(details);
    }
    Serial.println();
  }
}

// Add the method implementation after other onXXX methods
bool NetworkCore::registerDiscoveryHandler(NetworkDiscovery* discovery) {
  _discoveryHandler = discovery;
  Serial.println("[NetworkCore] Discovery handler registered");
  return true;
}