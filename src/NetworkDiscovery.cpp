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

  // Broadcast the message using the core
  return _core.broadcastMessage(MSG_TYPE_DISCOVERY, doc.as<JsonObject>());
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
    return;
  }

  // Add the sender to our peer list
  bool added = addPeer(senderId, senderMac);

  // Notify through callback if registered
  if (_discoveryCallback != NULL) {
    _discoveryCallback(senderId);
  }

  // Send a discovery response to let the sender know we exist
  StaticJsonDocument<128> doc;
  _core.sendMessage(senderId, MSG_TYPE_DISCOVERY_RESPONSE,
                    doc.as<JsonObject>());
}

bool NetworkDiscovery::addPeer(const char* boardId, const uint8_t* macAddress) {
  // Use the core's addPeer method
  return _core.addPeer(boardId, macAddress);
}