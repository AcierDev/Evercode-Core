/**
 * NetworkMessaging.h - Messaging functionality for ESP32 network communication
 * Created as part of the NetworkComm library refactoring
 *
 * This class handles topic-based and direct messaging between
 * ESP32 boards.
 */

#ifndef NetworkMessaging_h
#define NetworkMessaging_h

#include "NetworkCore.h"

// Maximum number of topic subscriptions
#define MAX_TOPIC_SUBSCRIPTIONS 20

// Callback function types
typedef void (*MessageCallback)(const char* sender, const char* topic,
                                const char* message);

class NetworkMessaging {
 public:
  /**
   * Constructor for NetworkMessaging
   *
   * @param core Reference to the NetworkCore instance
   */
  NetworkMessaging(NetworkCore& core);

  /**
   * Initialize the messaging service
   *
   * @return true if initialization was successful
   */
  bool begin();

  // ==================== Topic-based Messaging ====================
  /**
   * Publish a message to a topic that all boards can subscribe to
   *
   * @param topic The topic to publish to
   * @param message The message to publish
   * @return true if the message was sent successfully
   */
  bool publishTopic(const char* topic, const char* message);

  /**
   * Subscribe to a topic to receive messages
   *
   * @param topic The topic to subscribe to
   * @param callback Function to call when a message is received on this topic
   * @return true if the subscription was added successfully
   */
  bool subscribeTopic(const char* topic, MessageCallback callback);

  /**
   * Unsubscribe from a topic
   *
   * @param topic The topic to unsubscribe from
   * @return true if the subscription was removed successfully
   */
  bool unsubscribeTopic(const char* topic);

  // ==================== Direct Messaging ====================
  /**
   * Send a direct message to a specific board
   *
   * @param targetBoardId The ID of the board to send the message to
   * @param message The message to send
   * @return true if the message was sent successfully
   */
  bool sendMessageToBoardId(const char* targetBoardId, const char* message);

  /**
   * Receive direct messages from other boards
   *
   * @param callback Function to call when a direct message is received
   * @return true if the callback was set successfully
   */
  bool receiveMessagesFromBoards(MessageCallback callback);

  /**
   * Stop receiving direct messages
   *
   * @return true if the callback was cleared successfully
   */
  bool stopReceivingMessages();

  /**
   * Handle a topic message
   * Called internally by NetworkCore
   *
   * @param sender The ID of the board that sent the message
   * @param topic The topic of the message
   * @param message The message content
   * @return true if the message was handled successfully
   */
  bool handleTopicMessage(const char* sender, const char* topic,
                          const char* message);

  /**
   * Handle a direct message
   * Called internally by NetworkCore
   *
   * @param sender The ID of the board that sent the message
   * @param message The message content
   * @return true if the message was handled successfully
   */
  bool handleDirectMessage(const char* sender, const char* message);

 private:
  // Reference to the core network instance
  NetworkCore& _core;

  // Direct message callback
  MessageCallback _directMessageCallback;

  // Subscription management for topics
  struct TopicSubscription {
    char topic[32];
    MessageCallback callback;
    bool active;
  };

  TopicSubscription _topicSubscriptions[MAX_TOPIC_SUBSCRIPTIONS];
  int _topicSubscriptionCount;

  // Helper methods
  int findFreeTopicSubscriptionSlot();
  bool findMatchingTopicSubscription(const char* topic, int& index);
};

#endif