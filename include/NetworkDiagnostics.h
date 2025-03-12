/**
 * NetworkDiagnostics.h - Diagnostic functionality for ESP32 network
 * communication Created as part of the NetworkComm library refactoring
 *
 * This class provides diagnostic and debugging features for the
 * ESP-NOW based communication between ESP32 boards.
 */

#ifndef NetworkDiagnostics_h
#define NetworkDiagnostics_h

#include "NetworkCore.h"

// Diagnostic data collection interval
#define DIAGNOSTIC_COLLECTION_INTERVAL 5000  // 5 seconds

class NetworkDiagnostics {
 public:
  /**
   * Constructor for NetworkDiagnostics
   *
   * @param core Reference to the NetworkCore instance
   */
  NetworkDiagnostics(NetworkCore& core);

  /**
   * Initialize the diagnostics service
   *
   * @return true if initialization was successful
   */
  bool begin();

  /**
   * Update function that must be called regularly
   * This handles collecting diagnostic data
   */
  void update();

  /**
   * Enable or disable debug logging
   *
   * @param enable true to enable debug logging, false to disable
   * @return true if the setting was applied successfully
   */
  bool enableDebugLogging(bool enable);

  /**
   * Check if debug logging is enabled
   *
   * @return true if debug logging is enabled, false otherwise
   */
  bool isDebugLoggingEnabled();

  /**
   * Enable or disable verbose logging
   *
   * Verbose logging includes more detailed information than debug logging.
   *
   * @param enable true to enable verbose logging, false to disable
   * @return true if the setting was applied successfully
   */
  bool enableVerboseLogging(bool enable);

  /**
   * Check if verbose logging is enabled
   *
   * @return true if verbose logging is enabled, false otherwise
   */
  bool isVerboseLoggingEnabled();

  /**
   * Get the current network status as a JSON string
   *
   * @return A JSON string containing network status information
   */
  String getNetworkStatusJson();

  /**
   * Print the current network status to Serial
   */
  void printNetworkStatus();

  /**
   * Get the number of messages sent
   *
   * @return The number of messages sent since initialization
   */
  uint32_t getMessagesSent();

  /**
   * Get the number of messages received
   *
   * @return The number of messages received since initialization
   */
  uint32_t getMessagesReceived();

  /**
   * Get the number of message delivery failures
   *
   * @return The number of message delivery failures since initialization
   */
  uint32_t getMessageFailures();

  /**
   * Reset all diagnostic counters
   */
  void resetCounters();

 private:
  // Reference to the core network instance
  NetworkCore& _core;

  // Diagnostic counters
  uint32_t _messagesSent;
  uint32_t _messagesReceived;
  uint32_t _messageFailures;
  uint32_t _lastDiagnosticCollection;

  // Network statistics
  float _messageSuccessRate;
  uint32_t _averageResponseTime;

  // Helper methods
  void collectDiagnosticData();
};

#endif