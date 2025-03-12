# ESP-NOW Send Status Callback Example

This example demonstrates how to use the ESP-NOW send status callback feature added to the NetworkComm library. This feature provides immediate feedback on whether messages were successfully delivered to remote boards at the MAC layer, leveraging ESP-NOW's built-in delivery confirmation.

## Overview

ESP-NOW provides a built-in callback mechanism that reports whether a message was successfully delivered to the target device at the MAC layer. This is now exposed in the NetworkComm library, allowing you to:

1. Receive immediate notification of message delivery success or failure
2. Implement retry logic for failed transmissions
3. Monitor connection quality in real-time
4. Take appropriate actions based on delivery status

## Hardware Requirements

- 2 ESP32 boards
- 3 LEDs for the sender (status indicators)
- 1 push button for the sender
- 1 LED for the receiver

## Example Components

This example consists of two sketches:

1. **SendStatusCallback.ino** - The sender that controls a remote LED and monitors transmission status
2. **ReceiverBoard/ReceiverBoard.ino** - The receiver that accepts pin control commands

## Wiring - Sender Board

- Connect a button to pin 15 (with pull-up resistor)
- Connect an LED to pin 18 (with appropriate resistor) - Local LED state indicator
- Connect an LED to pin 19 (with appropriate resistor) - Success indicator
- Connect an LED to pin 21 (with appropriate resistor) - Failure indicator

## Wiring - Receiver Board

- Connect an LED to pin 13 (with appropriate resistor) - Remotely controlled LED
- Connect an LED to pin 19 (with appropriate resistor) - Status indicator

## Important Functions

### OnSendStatus Callback

```cpp
// Register a callback for send status notifications
netComm.onSendStatus(onSendStatus);

// Callback function implementation
void onSendStatus(const char* targetBoardId, uint8_t messageType, bool success) {
  // Handle success or failure
  if (success) {
    // Message was delivered successfully at the MAC layer
  } else {
    // Message delivery failed
  }
}
```

### OnSendFailure Callback

```cpp
// Register a callback specifically for failures
netComm.onSendFailure(onSendFailure);

// Callback function implementation
void onSendFailure(const char* targetBoardId, uint8_t messageType, uint8_t pin, uint8_t value) {
  // Handle failure case only - with additional details for pin control messages
  // This allows for specialized failure handling code without cluttering the main status callback

  // For pin control messages, the pin and value that failed to set are available
  if (messageType == MSG_TYPE_PIN_CONTROL) {
    // Take action specific to the failed pin update
  }

  // Implement retry logic or alternative actions
}
```

### Using the Callbacks

The example demonstrates three ways to use message delivery confirmation:

1. **Global send status monitoring** - Using the `onSendStatus` callback to monitor all messages
2. **Specific failure handling** - Using the `onSendFailure` callback for targeted error recovery
3. **Per-operation confirmation** - Using the callback parameter in `controlRemotePin`

## How It Works

1. The sender attempts to control an LED on the receiver board
2. The ESP-NOW protocol reports delivery status via callback
3. The sender displays the delivery status using LEDs:
   - Success LED flashes when a message is delivered successfully
   - Fail LED flashes when a message delivery fails (with longer duration for emphasis)
4. Statistics are displayed on the serial monitor showing success rate
5. Failure-specific actions can be implemented in the failure callback

## Troubleshooting

If you're experiencing high failure rates:

1. Ensure both devices are within range
2. Check if WiFi interference is affecting transmission
3. Consider using a different WiFi channel
4. Reduce the frequency of transmissions

## Library Integration

The new functionality is implemented through these main components:

1. The `esp_now_register_send_cb()` registration in the `begin()` method
2. A static callback handler that routes to instance methods
3. The `onSendStatus` method for registering general status callbacks
4. The `onSendFailure` method for registering failure-specific callbacks
5. The `handleSendStatus` method that processes callbacks and notifies users

## Failure Handling Strategies

The separate failure callback enables more sophisticated error handling:

1. **Automatic retries** - Implement a retry mechanism for failed messages
2. **Fallback behavior** - Switch to alternative actions when primary action fails
3. **Network diagnostics** - Log detailed information about failures for troubleshooting
4. **User feedback** - Provide immediate user feedback about communication issues

## Performance Considerations

The ESP-NOW send callback occurs in an interrupt context, so the actual work is delegated to the main loop via flags and state tracking to avoid blocking interrupts.

## Example Output

The serial monitor will show output like:

```
Send status for message to receiver, type: 1: SUCCESS
Success rate: 100% (1/1)

!! FAILURE !! Message to non-existent, type: 1, pin: 13, value: 1
Failure action: Could retry message or take alternative action
Target board not available - might need to rediscover
```

This indicates both successful and failed message delivery attempts.

## Advanced Usage

For more advanced applications, you can:

1. Implement automatic retries for failed transmissions
2. Monitor connection quality and adapt transmission parameters
3. Maintain delivery statistics for network health monitoring
4. Create a mesh network with reliable message delivery confirmation

## Compatibility

This functionality is available on all ESP32 boards that support ESP-NOW.
