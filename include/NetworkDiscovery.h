/**
 * NetworkDiscovery.h - Device discovery functionality for ESP32 network
 * communication Created as part of the NetworkComm library refactoring
 *
 * This class handles discovering other ESP32 boards on the network and
 * maintaining the list of available peers.
 */

#ifndef NetworkDiscovery_h
#define NetworkDiscovery_h

#include "NetworkCore.h"

// Discovery broadcast interval (ms)
#define INITIAL_DISCOVERY_INTERVAL 5000  // 5 seconds initially
#define ACTIVE_DISCOVERY_INTERVAL 20000  // 20 seconds during active discovery
#define STABLE_DISCOVERY_INTERVAL 60000  // 60 seconds after stable connection

// Callback function for discovery
typedef void (*DiscoveryCallback)(const char* boardId);

class NetworkDiscovery {
 public:
  /**
   * Constructor for NetworkDiscovery
   *
   * @param core Reference to the NetworkCore instance
   */
  NetworkDiscovery(NetworkCore& core);

  /**
   * Initialize the discovery service
   *
   * @return true if initialization was successful
   */
  bool begin();

  /**
   * Update function that must be called regularly
   * This handles periodic discovery broadcasts
   */
  void update();

  /**
   * Broadcast this board's presence to the network
   * Used for discovery by other boards
   *
   * @return true if the broadcast was successful
   */
  bool broadcastPresence();

  /**
   * Set a callback for when a new board is discovered
   *
   * @param callback Function to call when a new board is discovered
   * @return true if the callback was set successfully
   */
  bool onBoardDiscovered(DiscoveryCallback callback);

  /**
   * Check if a specific board is available on the network
   *
   * @param boardId The ID of the board to check
   * @return true if the board has been discovered, false otherwise
   */
  bool isBoardAvailable(const char* boardId);

  /**
   * Get the number of available boards on the network
   *
   * @return The number of discovered peer boards
   */
  int getAvailableBoardsCount();

  /**
   * Get the name of an available board by index
   *
   * @param index The index of the board (0 to getAvailableBoardsCount()-1)
   * @return The board ID as a String, or empty string if index is out of range
   */
  String getAvailableBoardName(int index);

  /**
   * Handle a discovery message from another board
   * Called internally by NetworkCore
   *
   * @param senderId The ID of the board that sent the discovery message
   * @param senderMac The MAC address of the board that sent the discovery
   * message
   */
  void handleDiscovery(const char* senderId, const uint8_t* senderMac);

  /**
   * Add a peer to the list of known boards
   *
   * @param boardId The ID of the board to add
   * @param macAddress The MAC address of the board to add
   * @return true if the peer was added successfully
   */
  bool addPeer(const char* boardId, const uint8_t* macAddress);

 private:
  // Reference to the core network instance
  NetworkCore& _core;

  // Discovery callback
  DiscoveryCallback _discoveryCallback;

  // Discovery state
  uint32_t _lastDiscoveryBroadcast;
  bool _firstMinuteDiscovery;
  bool _firstFiveMinutesDiscovery;
  uint32_t _discoveryStartTime;
};

#endif