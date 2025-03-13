# NetworkComm Pin Control Reliability Test

This example demonstrates how to test the reliability of the automatic retry functionality in the NetworkComm library. It sends 500 pin control messages and tracks how many successfully make it through, with detailed statistics on first-attempt successes, successes after retries, and failures despite retries.

## Hardware Requirements

You'll need two ESP32 boards:

1. **Sender board**: Runs the `PinControlReliabilityTest.ino` sketch
2. **Receiver board**: Runs the `ReceiverSketch.ino` sketch

### Recommended Connections

#### Sender Board

- Connect an LED to pin 2 (status indicator)
- Connect an LED to pin 19 (success indicator)
- Connect an LED to pin 21 (failure indicator)

#### Receiver Board

- Connect an LED to pin 13 (controlled by remote commands)
- Connect an LED to pin 2 (status indicator)

## Configuration

Before uploading the sketches, make sure to:

1. Update the WiFi credentials in both sketches:

   ```cpp
   const char* ssid = "YourWiFiSSID";
   const char* password = "YourWiFiPassword";
   ```

2. Optionally adjust the test parameters in the sender sketch:

   ```cpp
   const int TOTAL_MESSAGES = 500;  // Total number of messages to send
   const int SEND_DELAY_MS = 100;   // Delay between messages (ms)
   ```

3. Optionally adjust the retry settings in the sender sketch:
   ```cpp
   netComm.setPinControlMaxRetries(3);      // Maximum 3 retries
   netComm.setPinControlRetryDelay(500);    // 500ms between retries
   ```

## Running the Test

1. Upload the `ReceiverSketch.ino` to the receiver board first
2. Upload the `PinControlReliabilityTest.ino` to the sender board
3. Open the Serial Monitor for both boards (115200 baud)
4. The test will start automatically and run until all 500 messages have been sent
5. Final results will be displayed on the sender's Serial Monitor

## Understanding the Results

The test provides the following statistics:

- **Total messages sent**: Always 500 (or the configured value)
- **Messages succeeded on first attempt**: Number of messages that were delivered successfully without any retries
- **Messages succeeded with retries**: Number of messages that required one or more retries to be delivered successfully
- **Messages failed despite retries**: Number of messages that could not be delivered even after all retry attempts
- **Overall success rate**: Percentage of messages that were delivered successfully (with or without retries)

You can compare these statistics with the receiver's count of messages received to verify the accuracy of the test.

## Interpreting the Results

- **High first-attempt success rate**: Indicates good signal quality and low interference
- **High success with retries**: Indicates that the retry mechanism is working effectively in a challenging environment
- **High failure rate despite retries**: May indicate severe interference, distance issues, or other communication problems

## Troubleshooting

If you're experiencing high failure rates:

1. Reduce the distance between the boards
2. Increase the delay between messages (`SEND_DELAY_MS`)
3. Increase the maximum number of retries
4. Increase the retry delay to allow more time for interference to clear

## Advanced Testing

You can modify the test to simulate different conditions:

- Increase `SEND_DELAY_MS` to reduce network congestion
- Decrease `SEND_DELAY_MS` to stress-test the system
- Place obstacles between the boards to test signal penetration
- Introduce other WiFi or Bluetooth devices nearby to test interference handling
