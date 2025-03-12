/**
 * NetworkDiscovery.cpp - Device discovery functionality for ESP32 network
 * communication Created as part of the NetworkComm library refactoring
 */

#include "NetworkDiscovery.h"

// Constructor
NetworkDiscovery::NetworkDiscovery(NetworkCore& core) : _core(core) {
  _discoveryCallback = NULL;
  _lastDiscoveryBroadcast = 0;
  _firstMinuteDiscovery = true;
  _firstFiveMinutesDiscovery = true;
  _discoveryStartTime = 0;
}

bool NetworkDiscovery::begin() {
  _discoveryStartTime = millis();

  // Broadcast presence immediately to discover other boards
  broadcastPresence();

  return true;
}

void NetworkDiscovery::update() {
  if (!_core.isConnected()) return;

  uint32_t currentTime = millis();

  // Determine the appropriate discovery interval based on uptime
  uint32_t discoveryInterval =
      INITIAL_DISCOVERY_INTERVAL;  // 5 seconds initially

  // After the first minute, slow down to 20 seconds
  if (_firstMinuteDiscovery && (currentTime - _discoveryStartTime > 60000)) {
    _firstMinuteDiscovery = false;
    discoveryInterval = ACTIVE_DISCOVERY_INTERVAL;  // 20 seconds
  }

  // After five minutes, slow down further to 60 seconds
  if (_firstFiveMinutesDiscovery &&
      (currentTime - _discoveryStartTime > 300000)) {
    _firstFiveMinutesDiscovery = false;
    discoveryInterval = STABLE_DISCOVERY_INTERVAL;  // 60 seconds
  }

  // Check if it's time to broadcast presence
  if (currentTime - _lastDiscoveryBroadcast > discoveryInterval) {
    broadcastPresence();
    _lastDiscoveryBroadcast = currentTime;
  }
}

bool NetworkDiscovery::broadcastPresence() {
  if (!_core.isConnected()) return false;

  // Create a minimal discovery message
  StaticJsonDocument<128> doc;

  // No additional data needed for discovery broadcast

  // Add debug output before broadcasting
  Serial.print("[DISCOVERY] Broadcasting presence from board: ");
  Serial.println(_core._boardId);

  // Broadcast the message using the core
  bool result =
      _core.broadcastMessage(MSG_TYPE_DISCOVERY, doc.as<JsonObject>());

  // Log the result
  if (result) {
    Serial.println("[DISCOVERY] Broadcast sent successfully");
  } else {
    Serial.println("[DISCOVERY] Failed to send broadcast");
  }

  return result;
}

bool NetworkDiscovery::onBoardDiscovered(DiscoveryCallback callback) {
  _discoveryCallback = callback;
  return true;
}

bool NetworkDiscovery::isBoardAvailable(const char* boardId) {
  if (!_core.isConnected()) return false;

  // Check if this is our own board ID
  if (strcmp(boardId, _core._boardId) == 0) {
    return true;  // We are always available to ourselves
  }

  // Check all peers in the core's peer list
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_core._peers[i].active &&
        strcmp(_core._peers[i].boardId, boardId) == 0) {
      return true;
    }
  }

  return false;
}

int NetworkDiscovery::getAvailableBoardsCount() {
  // Return the count from the core
  return _core._peerCount;
}

String NetworkDiscovery::getAvailableBoardName(int index) {
  if (!_core.isConnected()) return String("");

  // Count through active peers to find the one at the requested index
  int count = 0;
  for (int i = 0; i < MAX_PEERS; i++) {
    if (_core._peers[i].active) {
      if (count == index) {
        return String(_core._peers[i].boardId);
      }
      count++;
    }
  }

  return String("");  // Not found
}

void NetworkDiscovery::handleDiscovery(const char* senderId,
                                       const uint8_t* senderMac) {
  // Don't process discovery messages from ourselves
  if (strcmp(senderId, _core._boardId) == 0) {
    Serial.println("[DISCOVERY] Ignoring discovery from self");
    return;
  }

  Serial.print("[DISCOVERY] Received discovery from board: ");
  Serial.println(senderId);

  // Format MAC address
  char macStr[18];
  sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", senderMac[0], senderMac[1],
          senderMac[2], senderMac[3], senderMac[4], senderMac[5]);
  Serial.print("[DISCOVERY] Sender MAC: ");
  Serial.println(macStr);

  // Add the sender to our peer list
  bool added = addPeer(senderId, senderMac);
  Serial.print("[DISCOVERY] Peer added: ");
  Serial.println(added ? "YES" : "NO");

  // Notify through callback if registered
  if (_discoveryCallback != NULL) {
    _discoveryCallback(senderId);
    Serial.println("[DISCOVERY] Discovery callback executed");
  } else {
    Serial.println("[DISCOVERY] No discovery callback registered");
  }

  // Send a discovery response to let the sender know we exist
  StaticJsonDocument<128> doc;

  Serial.print("[DISCOVERY] Sending discovery response to: ");
  Serial.println(senderId);

  bool sent = _core.sendMessage(senderId, MSG_TYPE_DISCOVERY_RESPONSE,
                                doc.as<JsonObject>());

  Serial.print("[DISCOVERY] Response sent: ");
  Serial.println(sent ? "YES" : "NO");
}

bool NetworkDiscovery::addPeer(const char* boardId, const uint8_t* macAddress) {
  // Use the core's addPeer method
  return _core.addPeer(boardId, macAddress);
}