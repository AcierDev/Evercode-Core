/**
 * NetworkComm MQTT Broker Example
 *
 * This example demonstrates how to set up an ESP32 as an MQTT broker
 * for the NetworkComm library. This allows boards to communicate without
 * requiring an external MQTT broker.
 *
 * Note: This example requires the uMQTTBroker library:
 * https://github.com/martin-ger/uMQTTBroker
 */

#include <Arduino.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <uMQTTBroker.h>

#include "NetworkComm.h"

// Network configuration
const char* ssid = "YourWiFiSSID";
const char* password = "YourWiFiPassword";

// Board ID
const char* boardId = "mqtt-broker";

// MQTT broker port
const int mqttPort = 1883;

// Custom MQTT broker class
class CustomMQTTBroker : public uMQTTBroker {
 public:
  // Constructor
  CustomMQTTBroker() : uMQTTBroker() {}

  // Called when a client connects
  virtual bool onConnect(IPAddress addr, uint16_t client_count) {
    Serial.println("Client connected: " + addr.toString());
    return true;
  }

  // Called when a client disconnects
  virtual void onDisconnect(IPAddress addr, String client_id) {
    Serial.println("Client disconnected: " + addr.toString() + " " + client_id);
  }

  // Called when a message is received
  virtual bool onData(String topic, const char* data, uint32_t length) {
    char* message = new char[length + 1];
    memcpy(message, data, length);
    message[length] = '\0';

    Serial.println("Message received: " + topic + " = " + String(message));

    delete[] message;
    return true;
  }
};

// MQTT broker instance
CustomMQTTBroker broker;

// NetworkComm instance
NetworkComm netComm;

// Callback for received messages
void onMessageReceived(const char* sender, const char* topic,
                       const char* message) {
  Serial.print("Message from ");
  Serial.print(sender);
  Serial.print(" on topic ");
  Serial.print(topic);
  Serial.print(": ");
  Serial.println(message);
}

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("NetworkComm MQTT Broker Example");

  // Connect to WiFi
  WiFi.begin(ssid, password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Setup mDNS
  if (!MDNS.begin(boardId)) {
    Serial.println("Error setting up mDNS responder!");
    while (1) {
      delay(1000);
    }
  }

  // Advertise MQTT service
  MDNS.addService("mqtt", "tcp", mqttPort);

  // Start MQTT broker
  broker.init();
  Serial.println("MQTT broker started");

  // Initialize NetworkComm
  if (netComm.begin(ssid, password, boardId)) {
    Serial.println("NetworkComm initialized");
  } else {
    Serial.println("Failed to initialize NetworkComm");
    while (1) {
      delay(1000);
    }
  }

  // Subscribe to test topic
  netComm.subscribe("test/topic", onMessageReceived);

  Serial.println("Setup complete");
}

void loop() {
  // Update NetworkComm
  netComm.update();

  // Publish a message every 5 seconds
  static unsigned long lastPublishTime = 0;
  if (millis() - lastPublishTime > 5000) {
    lastPublishTime = millis();

    // Publish uptime
    String message = "Broker uptime: " + String(millis() / 1000) + " seconds";
    netComm.publish("test/uptime", message.c_str());

    Serial.println("Published uptime message");
  }

  // Small delay
  delay(10);
}