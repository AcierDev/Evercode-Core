/**
 * NetworkDiagnostics.cpp - Diagnostic functionality for ESP32 network
 * communication Created as part of the NetworkComm library refactoring
 */

#include "NetworkDiagnostics.h"

// Constructor
NetworkDiagnostics::NetworkDiagnostics(NetworkCore& core) : _core(core) {
  _messagesSent = 0;
  _messagesReceived = 0;
  _messageFailures = 0;
  _lastDiagnosticCollection = 0;
  _messageSuccessRate = 0.0;
  _averageResponseTime = 0;
}

bool NetworkDiagnostics::begin() {
  // No specific initialization needed
  _lastDiagnosticCollection = millis();
  return true;
}

void NetworkDiagnostics::update() {
  if (!_core.isConnected()) return;

  uint32_t currentTime = millis();

  // Collect diagnostic data periodically
  if (currentTime - _lastDiagnosticCollection >
      DIAGNOSTIC_COLLECTION_INTERVAL) {
    collectDiagnosticData();
    _lastDiagnosticCollection = currentTime;
  }
}

bool NetworkDiagnostics::enableDebugLogging(bool enable) {
  // Access the debug logging flag in the core
  _core._debugLoggingEnabled = enable;

  // Output a message if enabling (it will only show if debug is now on)
  if (enable) {
    Serial.println("[NetworkDiagnostics] Debug logging enabled");
  }

  return true;
}

bool NetworkDiagnostics::isDebugLoggingEnabled() {
  return _core._debugLoggingEnabled;
}

bool NetworkDiagnostics::enableVerboseLogging(bool enable) {
  // Access the verbose logging flag in the core
  _core._verboseLoggingEnabled = enable;

  // Output a debug message about the change
  char debugMsg[50];
  sprintf(debugMsg, "Verbose logging %s", enable ? "enabled" : "disabled");
  _core.debugLog(debugMsg);

  return true;
}

bool NetworkDiagnostics::isVerboseLoggingEnabled() {
  return _core._verboseLoggingEnabled;
}

String NetworkDiagnostics::getNetworkStatusJson() {
  if (!_core.isConnected()) {
    return "{\"status\":\"disconnected\"}";
  }

  // Create a JSON document with network status
  StaticJsonDocument<512> doc;

  // Basic status
  doc["status"] = "connected";
  doc["board_id"] = _core._boardId;

  // Get MAC address as string
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", _core._macAddress[0],
          _core._macAddress[1], _core._macAddress[2], _core._macAddress[3],
          _core._macAddress[4], _core._macAddress[5]);
  doc["mac_address"] = macStr;

  // Connection stats
  doc["peers_count"] = _core._peerCount;
  doc["messages_sent"] = _messagesSent;
  doc["messages_received"] = _messagesReceived;
  doc["message_failures"] = _messageFailures;
  doc["success_rate"] = _messageSuccessRate;
  doc["avg_response_time_ms"] = _averageResponseTime;

  // Create an array of peers
  JsonArray peers = doc.createNestedArray("peers");
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_core._peers[i].active) {
      JsonObject peer = peers.createNestedObject();
      peer["board_id"] = _core._peers[i].boardId;

      // Get MAC address as string
      char peerMacStr[18];
      sprintf(peerMacStr, "%02X:%02X:%02X:%02X:%02X:%02X",
              _core._peers[i].macAddress[0], _core._peers[i].macAddress[1],
              _core._peers[i].macAddress[2], _core._peers[i].macAddress[3],
              _core._peers[i].macAddress[4], _core._peers[i].macAddress[5]);
      peer["mac_address"] = peerMacStr;

      // Calculate time since last seen
      uint32_t lastSeenSeconds = (millis() - _core._peers[i].lastSeen) / 1000;
      peer["last_seen_seconds"] = lastSeenSeconds;
    }
  }

  // Serialize to JSON string
  String jsonStr;
  serializeJson(doc, jsonStr);
  return jsonStr;
}

void NetworkDiagnostics::printNetworkStatus() {
  if (!_core.isConnected()) {
    Serial.println("[NetworkDiagnostics] Status: Disconnected");
    return;
  }

  // Print basic status
  Serial.println("\n===== Network Status =====");
  Serial.print("Board ID: ");
  Serial.println(_core._boardId);

  // Print MAC address
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", _core._macAddress[0],
          _core._macAddress[1], _core._macAddress[2], _core._macAddress[3],
          _core._macAddress[4], _core._macAddress[5]);
  Serial.print("MAC Address: ");
  Serial.println(macStr);

  // Print connection stats
  Serial.print("Peers: ");
  Serial.println(_core._peerCount);
  Serial.print("Messages Sent: ");
  Serial.println(_messagesSent);
  Serial.print("Messages Received: ");
  Serial.println(_messagesReceived);
  Serial.print("Message Failures: ");
  Serial.println(_messageFailures);
  Serial.print("Success Rate: ");
  Serial.print(_messageSuccessRate);
  Serial.println("%");
  Serial.print("Avg Response Time: ");
  Serial.print(_averageResponseTime);
  Serial.println(" ms");

  // Print peers
  Serial.println("\n--- Peers ---");
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_core._peers[i].active) {
      Serial.print("Board: ");
      Serial.print(_core._peers[i].boardId);

      // Calculate time since last seen
      uint32_t lastSeenSeconds = (millis() - _core._peers[i].lastSeen) / 1000;
      Serial.print(", Last Seen: ");
      Serial.print(lastSeenSeconds);
      Serial.println(" sec ago");
    }
  }

  Serial.println("==========================\n");
}

uint32_t NetworkDiagnostics::getMessagesSent() { return _messagesSent; }

uint32_t NetworkDiagnostics::getMessagesReceived() { return _messagesReceived; }

uint32_t NetworkDiagnostics::getMessageFailures() { return _messageFailures; }

void NetworkDiagnostics::resetCounters() {
  _messagesSent = 0;
  _messagesReceived = 0;
  _messageFailures = 0;
  _messageSuccessRate = 0.0;
  _averageResponseTime = 0;
}

void NetworkDiagnostics::collectDiagnosticData() {
  // In a real implementation, we would collect data from various sources
  // For now, we just calculate some simple stats

  // Count active tracked messages for pending calculations
  int pendingMessages = 0;
  for (int i = 0; i < NetworkCore::MAX_TRACKED_MESSAGES; i++) {
    if (_core._trackedMessages[i].active) {
      pendingMessages++;
    }
  }

  // Calculate success rate
  if (_messagesSent > 0) {
    _messageSuccessRate =
        100.0 * (_messagesSent - _messageFailures) / _messagesSent;
  } else {
    _messageSuccessRate = 0.0;
  }

  // In a real implementation, we would calculate average response time
  // based on acknowledgement timestamps
  _averageResponseTime = 0;  // Placeholder

  // Debug output if enabled
  if (_core._debugLoggingEnabled) {
    char debugMsg[100];
    sprintf(
        debugMsg,
        "Messages: %lu sent, %lu received, %lu failures, %.1f%% success rate",
        _messagesSent, _messagesReceived, _messageFailures,
        _messageSuccessRate);
    _core.debugLog("Diagnostic collection", debugMsg);
  }
}