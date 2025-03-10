/**
 * NetworkComm.cpp - Library for communication between ESP32 boards
 * Created by Claude, 2023
 */

#include "NetworkComm.h"

// Local MQTT broker port
#define MQTT_PORT 1883

// Topic prefixes
#define TOPIC_PREFIX "netcomm/"
#define DISCOVERY_TOPIC TOPIC_PREFIX "discovery"
#define PIN_TOPIC TOPIC_PREFIX "pin/"
#define MESSAGE_TOPIC TOPIC_PREFIX "msg/"
#define SERIAL_TOPIC TOPIC_PREFIX "serial/"
#define DIRECT_TOPIC TOPIC_PREFIX "direct/"

// Forward declaration for MQTT callback
void mqttCallback(char* topic, byte* payload, unsigned int length);

// Constructor
NetworkComm::NetworkComm() {
  _isConnected = false;
  _subscriptionCount = 0;
  _directMessageCallback = NULL;
  _serialDataCallback = NULL;

  // Initialize subscriptions
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    _subscriptions[i].active = false;
  }
}

// Initialize with WiFi
bool NetworkComm::begin(const char* ssid, const char* password,
                        const char* boardId) {
  // Store board ID
  strncpy(_boardId, boardId, sizeof(_boardId) - 1);
  _boardId[sizeof(_boardId) - 1] = '\0';

  // Connect to WiFi
  WiFi.begin(ssid, password);

  // Wait for connection (with timeout)
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    if (millis() - startTime > 10000) {
      return false;  // Connection timeout
    }
  }

  // Setup mDNS
  setupMDNS();

  // Setup MQTT client
  _mqttClient.setClient(_wifiClient);
  _mqttClient.setServer("mqtt-broker.local",
                        MQTT_PORT);  // Use mDNS to find broker
  _mqttClient.setCallback(mqttCallback);

  // Connect to MQTT broker
  String clientId = "netcomm-";
  clientId += _boardId;
  if (_mqttClient.connect(clientId.c_str())) {
    // Subscribe to discovery topic
    _mqttClient.subscribe(DISCOVERY_TOPIC);

    // Subscribe to direct messages
    String directTopic = DIRECT_TOPIC;
    directTopic += _boardId;
    _mqttClient.subscribe(directTopic.c_str());

    // Announce presence
    _mqttClient.publish(DISCOVERY_TOPIC, _boardId);

    _isConnected = true;
    return true;
  }

  return false;
}

// Main loop function - must be called in loop()
void NetworkComm::update() {
  if (!_isConnected) return;

  // Handle MQTT messages
  _mqttClient.loop();

  // Check for new boards
  checkForNewBoards();
}

// Setup mDNS
void NetworkComm::setupMDNS() {
  if (!MDNS.begin(_boardId)) {
    return;
  }

  // Advertise MQTT service
  MDNS.addService("mqtt", "tcp", MQTT_PORT);
}

// Check for new boards
void NetworkComm::checkForNewBoards() {
  // This would typically query mDNS for services
  // Implementation depends on platform
}

// Process incoming MQTT messages
void NetworkComm::processIncomingMessage(const char* topic, byte* payload,
                                         unsigned int length) {
  // Ensure null-terminated payload
  char* message = new char[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';

  // Calculate an appropriate buffer size based on the message length
  // For JSON, we typically need 1.5x the raw message size plus some overhead
  const size_t capacity = length * 1.5 + 64;
  DynamicJsonDocument doc(capacity);

  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    // Handle JSON parsing error
    Serial.print(F("JSON parsing failed: "));
    Serial.println(error.c_str());
    delete[] message;
    return;
  }

  // Process based on topic
  if (strncmp(topic, DISCOVERY_TOPIC, strlen(DISCOVERY_TOPIC)) == 0) {
    // Handle discovery message
  } else if (strncmp(topic, PIN_TOPIC, strlen(PIN_TOPIC)) == 0) {
    // Handle pin control/subscription
    const char* sender = doc["sender"];
    uint8_t pin = doc["pin"];
    uint8_t value = doc["value"];
    uint8_t msgType = doc["type"];

    // Find matching subscriptions
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
      if (_subscriptions[i].active &&
          _subscriptions[i].type == MSG_TYPE_PIN_SUBSCRIBE &&
          strcmp(_subscriptions[i].targetBoard, sender) == 0 &&
          _subscriptions[i].pin == pin) {
        PinChangeCallback callback =
            (PinChangeCallback)_subscriptions[i].callback;
        if (callback) {
          callback(sender, pin, value);
        }
      }
    }
  } else if (strncmp(topic, MESSAGE_TOPIC, strlen(MESSAGE_TOPIC)) == 0) {
    // Handle pub/sub messages
    const char* sender = doc["sender"];
    const char* msgTopic = doc["topic"];
    const char* msgContent = doc["message"];

    // Find matching subscriptions
    for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
      if (_subscriptions[i].active &&
          _subscriptions[i].type == MSG_TYPE_MESSAGE &&
          strcmp(_subscriptions[i].topic, msgTopic) == 0) {
        MessageCallback callback = (MessageCallback)_subscriptions[i].callback;
        if (callback) {
          callback(sender, msgTopic, msgContent);
        }
      }
    }
  } else if (strncmp(topic, SERIAL_TOPIC, strlen(SERIAL_TOPIC)) == 0) {
    // Handle serial data
    const char* sender = doc["sender"];
    const char* data = doc["data"];

    if (_serialDataCallback) {
      _serialDataCallback(sender, data);
    }
  } else if (strncmp(topic, DIRECT_TOPIC, strlen(DIRECT_TOPIC)) == 0) {
    // Handle direct messages
    const char* sender = doc["sender"];
    const char* msgContent = doc["message"];

    if (_directMessageCallback) {
      _directMessageCallback(sender, NULL, msgContent);
    }
  }

  delete[] message;
}

// Send a message to a specific board
void NetworkComm::sendMessage(const char* targetBoard, uint8_t messageType,
                              const JsonObject& doc) {
  if (!_isConnected) return;

  // Reserve memory for the outgoing document (original doc + sender and type
  // fields) Plus extra margin for additional fields
  const size_t capacity = JSON_OBJECT_SIZE(2) + measureJson(doc) + 30;
  DynamicJsonDocument outDoc(capacity);
  outDoc.set(doc);  // Copy contents from original doc

  // Add sender information
  outDoc["sender"] = _boardId;
  outDoc["type"] = messageType;

  // Serialize to JSON
  String jsonStr;
  serializeJson(outDoc, jsonStr);

  // Determine topic based on message type
  String topic;

  switch (messageType) {
    case MSG_TYPE_PIN_CONTROL:
    case MSG_TYPE_PIN_PUBLISH:
      topic = PIN_TOPIC;
      topic += targetBoard;
      break;

    case MSG_TYPE_MESSAGE:
      topic = MESSAGE_TOPIC;
      topic += doc["topic"].as<const char*>();
      break;

    case MSG_TYPE_SERIAL_DATA:
      topic = SERIAL_TOPIC;
      break;

    case MSG_TYPE_DIRECT_MESSAGE:
      topic = DIRECT_TOPIC;
      topic += targetBoard;
      break;

    default:
      return;  // Unknown message type
  }

  // Publish message
  _mqttClient.publish(topic.c_str(), jsonStr.c_str());
}

// Check if connected to network
bool NetworkComm::isConnected() { return _isConnected; }

// Check if a specific board is available
bool NetworkComm::isBoardAvailable(const char* boardId) {
  // This would check mDNS records
  // Implementation depends on platform
  return true;  // Placeholder
}

// Get count of available boards
int NetworkComm::getAvailableBoardsCount() {
  // This would count mDNS records
  // Implementation depends on platform
  return 1;  // Placeholder
}

// Get name of available board by index
String NetworkComm::getAvailableBoardName(int index) {
  // This would return board name from mDNS records
  // Implementation depends on platform
  return "board";  // Placeholder
}

// Set pin value on remote board
bool NetworkComm::setPinValue(const char* targetBoard, uint8_t pin,
                              uint8_t value) {
  if (!_isConnected) return false;

  // For a simple pin value object with pin and value fields
  const size_t capacity = JSON_OBJECT_SIZE(2);
  DynamicJsonDocument doc(capacity);
  doc["pin"] = pin;
  doc["value"] = value;

  sendMessage(targetBoard, MSG_TYPE_PIN_CONTROL, doc.as<JsonObject>());
  return true;
}

// Get pin value from remote board (this would be async in reality)
uint8_t NetworkComm::getPinValue(const char* targetBoard, uint8_t pin) {
  // This would need to be implemented with a request/response pattern
  // For simplicity, we're returning 0
  return 0;
}

// Subscribe to pin changes on remote board
bool NetworkComm::subscribeToPinChange(const char* targetBoard, uint8_t pin,
                                       PinChangeCallback callback) {
  if (!_isConnected) return false;

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

  // Subscribe to pin topic
  String topic = PIN_TOPIC;
  topic += targetBoard;
  _mqttClient.subscribe(topic.c_str());

  // For a simple pin object with just one field
  const size_t capacity = JSON_OBJECT_SIZE(1);
  DynamicJsonDocument doc(capacity);
  doc["pin"] = pin;

  sendMessage(targetBoard, MSG_TYPE_PIN_SUBSCRIBE, doc.as<JsonObject>());
  return true;
}

// Unsubscribe from pin changes
bool NetworkComm::unsubscribeFromPinChange(const char* targetBoard,
                                           uint8_t pin) {
  if (!_isConnected) return false;

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

  // Calculate capacity for topic and message fields
  const size_t capacity =
      JSON_OBJECT_SIZE(2) + strlen(topic) + strlen(message) + 20;
  DynamicJsonDocument doc(capacity);
  doc["topic"] = topic;
  doc["message"] = message;

  sendMessage(NULL, MSG_TYPE_MESSAGE, doc.as<JsonObject>());
  return true;
}

// Subscribe to topic
bool NetworkComm::subscribe(const char* topic, MessageCallback callback) {
  if (!_isConnected) return false;

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

  // Subscribe to message topic
  String mqttTopic = MESSAGE_TOPIC;
  mqttTopic += topic;
  _mqttClient.subscribe(mqttTopic.c_str());

  return true;
}

// Unsubscribe from topic
bool NetworkComm::unsubscribe(const char* topic) {
  if (!_isConnected) return false;

  // Find matching subscription
  for (int i = 0; i < MAX_SUBSCRIPTIONS; i++) {
    if (_subscriptions[i].active &&
        _subscriptions[i].type == MSG_TYPE_MESSAGE &&
        strcmp(_subscriptions[i].topic, topic) == 0) {
      _subscriptions[i].active = false;

      // Unsubscribe from MQTT topic
      String mqttTopic = MESSAGE_TOPIC;
      mqttTopic += topic;
      _mqttClient.unsubscribe(mqttTopic.c_str());

      return true;
    }
  }

  return false;
}

// Publish serial data
bool NetworkComm::publishSerialData(const char* data) {
  if (!_isConnected) return false;

  // Calculate capacity based on data size
  const size_t capacity = JSON_OBJECT_SIZE(1) + strlen(data) + 10;
  DynamicJsonDocument doc(capacity);
  doc["data"] = data;

  sendMessage(NULL, MSG_TYPE_SERIAL_DATA, doc.as<JsonObject>());
  return true;
}

// Subscribe to serial data
bool NetworkComm::subscribeToSerialData(SerialDataCallback callback) {
  if (!_isConnected) return false;

  _serialDataCallback = callback;

  // Subscribe to serial topic
  _mqttClient.subscribe(SERIAL_TOPIC);

  return true;
}

// Unsubscribe from serial data
bool NetworkComm::unsubscribeFromSerialData() {
  if (!_isConnected) return false;

  _serialDataCallback = NULL;

  // Unsubscribe from serial topic
  _mqttClient.unsubscribe(SERIAL_TOPIC);

  return true;
}

// Send direct message to specific board
bool NetworkComm::sendDirectMessage(const char* targetBoard,
                                    const char* message) {
  if (!_isConnected) return false;

  // Calculate the capacity needed for the message
  // JSON_OBJECT_SIZE(2) accounts for the {"message": "value", "sender":
  // "value"} structure
  const size_t capacity =
      JSON_OBJECT_SIZE(2) + strlen(message) + strlen(_boardId) + 20;
  DynamicJsonDocument doc(capacity);

  doc["message"] = message;

  sendMessage(targetBoard, MSG_TYPE_DIRECT_MESSAGE, doc.as<JsonObject>());
  return true;
}

// Set callback for direct messages
bool NetworkComm::setDirectMessageCallback(MessageCallback callback) {
  _directMessageCallback = callback;
  return true;
}

// MQTT callback function
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // This needs to be connected to the NetworkComm instance
  // In a real implementation, we would use a static pointer to the instance
  // For now, this is just a placeholder
}