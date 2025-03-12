/**
 * NetworkMessaging.cpp - Messaging functionality for ESP32 network
 * communication Created as part of the NetworkComm library refactoring
 */

#include "NetworkMessaging.h"

// Constructor
NetworkMessaging::NetworkMessaging(NetworkCore& core) : _core(core) {
  _directMessageCallback = NULL;
  _topicSubscriptionCount = 0;

  // Initialize subscriptions
  for (int i = 0; i < MAX_TOPIC_SUBSCRIPTIONS; i++) {
    _topicSubscriptions[i].active = false;
  }
}

bool NetworkMessaging::begin() {
  // No specific initialization needed
  return true;
}

// ==================== Topic-based Messaging ====================

bool NetworkMessaging::publishTopic(const char* topic, const char* message) {
  if (!_core.isConnected()) return false;
  if (!topic || !message) return false;

  // Prepare the message
  StaticJsonDocument<256> doc;
  doc["topic"] = topic;
  doc["message"] = message;

  // Broadcast the message
  return _core.broadcastMessage(MSG_TYPE_MESSAGE, doc.as<JsonObject>());
}

bool NetworkMessaging::subscribeTopic(const char* topic,
                                      MessageCallback callback) {
  if (!_core.isConnected()) return false;
  if (!topic || !callback) return false;

  // Find a free subscription slot
  int slot = findFreeTopicSubscriptionSlot();
  if (slot == -1) return false;  // No free slots

  // Store the subscription
  strncpy(_topicSubscriptions[slot].topic, topic,
          sizeof(_topicSubscriptions[slot].topic) - 1);
  _topicSubscriptions[slot].topic[sizeof(_topicSubscriptions[slot].topic) - 1] =
      '\0';
  _topicSubscriptions[slot].callback = callback;
  _topicSubscriptions[slot].active = true;

  if (_topicSubscriptionCount < MAX_TOPIC_SUBSCRIPTIONS)
    _topicSubscriptionCount++;

  return true;
}

bool NetworkMessaging::unsubscribeTopic(const char* topic) {
  if (!_core.isConnected()) return false;
  if (!topic) return false;

  // Find and remove the matching subscriptions
  int index = -1;
  if (findMatchingTopicSubscription(topic, index)) {
    _topicSubscriptions[index].active = false;
    return true;
  }

  return false;  // Subscription not found
}

// ==================== Direct Messaging ====================

bool NetworkMessaging::sendMessageToBoardId(const char* targetBoardId,
                                            const char* message) {
  if (!_core.isConnected()) return false;
  if (!targetBoardId || !message) return false;

  // Prepare the message
  StaticJsonDocument<256> doc;
  doc["message"] = message;

  // Send the message
  return _core.sendMessage(targetBoardId, MSG_TYPE_DIRECT_MESSAGE,
                           doc.as<JsonObject>());
}

bool NetworkMessaging::receiveMessagesFromBoards(MessageCallback callback) {
  _directMessageCallback = callback;
  return true;
}

bool NetworkMessaging::stopReceivingMessages() {
  _directMessageCallback = NULL;
  return true;
}

// ==================== Message Handlers ====================

bool NetworkMessaging::handleTopicMessage(const char* sender, const char* topic,
                                          const char* message) {
  if (!sender || !topic || !message) return false;

  bool handled = false;

  // Find and call matching topic subscriptions
  for (int i = 0; i < MAX_TOPIC_SUBSCRIPTIONS; i++) {
    if (_topicSubscriptions[i].active &&
        strcmp(_topicSubscriptions[i].topic, topic) == 0) {
      _topicSubscriptions[i].callback(sender, topic, message);
      handled = true;
    }
  }

  return handled;
}

bool NetworkMessaging::handleDirectMessage(const char* sender,
                                           const char* message) {
  if (!sender || !message) return false;

  // Call the direct message callback if registered
  if (_directMessageCallback) {
    _directMessageCallback(sender, NULL, message);
    return true;
  }

  return false;
}

// ==================== Helper Methods ====================

int NetworkMessaging::findFreeTopicSubscriptionSlot() {
  for (int i = 0; i < MAX_TOPIC_SUBSCRIPTIONS; i++) {
    if (!_topicSubscriptions[i].active) {
      return i;
    }
  }
  return -1;  // No free slots
}

bool NetworkMessaging::findMatchingTopicSubscription(const char* topic,
                                                     int& index) {
  for (int i = 0; i < MAX_TOPIC_SUBSCRIPTIONS; i++) {
    if (_topicSubscriptions[i].active &&
        strcmp(_topicSubscriptions[i].topic, topic) == 0) {
      index = i;
      return true;
    }
  }
  return false;  // No match found
}