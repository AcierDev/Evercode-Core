/**
 * NetworkSerial.h - Serial data forwarding functionality for ESP32 network
 * communication Created as part of the NetworkComm library refactoring
 *
 * This class handles forwarding serial data between ESP32 boards.
 */

#ifndef NetworkSerial_h
#define NetworkSerial_h

#include "NetworkCore.h"

// Maximum serial data buffer size
#define MAX_SERIAL_DATA_SIZE 200

// Callback function types
typedef void (*SerialDataCallback)(const char* sender, const char* data);

class NetworkSerial {
 public:
  /**
   * Constructor for NetworkSerial
   *
   * @param core Reference to the NetworkCore instance
   */
  NetworkSerial(NetworkCore& core);

  /**
   * Initialize the serial forwarding service
   *
   * @return true if initialization was successful
   */
  bool begin();

  /**
   * Forward serial data to all boards on the network
   *
   * @param data The data to forward
   * @return true if the data was sent successfully
   */
  bool forwardSerialData(const char* data);

  /**
   * Receive serial data from other boards
   *
   * @param callback Function to call when serial data is received
   * @return true if the callback was set successfully
   */
  bool receiveSerialData(SerialDataCallback callback);

  /**
   * Stop receiving serial data
   *
   * @return true if the callback was cleared successfully
   */
  bool stopReceivingSerialData();

  /**
   * Handle serial data message
   * Called internally by NetworkCore
   *
   * @param sender The ID of the board that sent the data
   * @param data The serial data
   * @return true if the data was handled successfully
   */
  bool handleSerialDataMessage(const char* sender, const char* data);

  /**
   * Enable automatic forwarding of local Serial input
   * This will read from Serial and forward to all boards
   *
   * @param enable true to enable, false to disable
   * @return true if the setting was applied successfully
   */
  bool enableAutoForwarding(bool enable);

  /**
   * Update function that must be called regularly if auto-forwarding is enabled
   * This handles reading from Serial and forwarding
   */
  void update();

 private:
  // Reference to the core network instance
  NetworkCore& _core;

  // Serial data callback
  SerialDataCallback _serialDataCallback;

  // Auto-forwarding state
  bool _autoForwardingEnabled;
  char _serialBuffer[MAX_SERIAL_DATA_SIZE];
  int _serialBufferIndex;
  unsigned long _lastSerialRead;
};

#endif