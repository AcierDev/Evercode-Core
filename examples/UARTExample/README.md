# PinComm UART Communication Examples

This directory contains examples demonstrating how to use the PinComm library with UART communication between ESP32 or Arduino boards.

## Overview

The PinComm library has been updated to use UART (Serial) communication instead of direct pin-based communication. This provides more reliable and efficient communication between boards.

Two example sketches are provided:

1. **UARTExample.ino** - For ESP32 boards with multiple hardware serial ports
2. **SoftwareSerialExample.ino** - For Arduino boards using SoftwareSerial

## Hardware Setup

### Basic Wiring

To connect two boards for UART communication, you need to make these connections:

```
Board A                Board B
-------                -------
TX      -----------→   RX
RX      ←-----------   TX
GND     -----------    GND
```

**Important Notes:**

- TX (transmit) from one board connects to RX (receive) on the other board
- The connections are crossed (TX→RX, RX→TX)
- GND (ground) must be connected between the boards

### ESP32 Hardware Serial Example

For ESP32 boards, we use the second hardware serial port (Serial2):

```
ESP32 Board A          ESP32 Board B
-------------          -------------
GPIO17 (TX2) -----→    GPIO16 (RX2)
GPIO16 (RX2) ←-----    GPIO17 (TX2)
GND          -----     GND
```

### Arduino SoftwareSerial Example

For Arduino boards, we use SoftwareSerial:

```
Arduino Board A        Arduino Board B
--------------        --------------
Pin 11 (TX)   -----→   Pin 10 (RX)
Pin 10 (RX)   ←-----   Pin 11 (TX)
GND           -----    GND
```

## Using the Examples

### Step 1: Upload the Sketches

1. Choose which example to use based on your hardware
2. For one board, set `IS_BOARD_A` to `true` in the sketch
3. For the other board, set `IS_BOARD_A` to `false`
4. Upload the appropriate sketch to each board

### Step 2: Monitor Communication

1. Open the Serial Monitor for both boards (115200 baud)
2. You should see initialization messages
3. After a few seconds, the boards should discover each other
4. Board A will send messages and control Board B's LED

## Key Features Demonstrated

- **Board Discovery**: Boards automatically discover each other
- **Direct Messaging**: Send messages directly to a specific board
- **Remote Pin Control**: Control pins on a remote board
- **Topic-based Messaging**: Publish and subscribe to topics
- **Acknowledgements**: Confirm message delivery

## Troubleshooting

### No Communication Between Boards

1. **Check Wiring**: Ensure TX→RX, RX→TX, and GND connections are correct
2. **Verify Baud Rate**: Both boards must use the same baud rate (9600 in examples)
3. **Check Power**: Ensure both boards are powered properly
4. **Logic Levels**: Some boards use 5V logic, others 3.3V. You may need level shifters

### Board Discovery Issues

1. **Wait Longer**: Discovery can take up to 30 seconds
2. **Enable Debug Logging**: Set `pinComm.enableDebugLogging(true)` to see more details
3. **Check Board IDs**: Ensure each board has a unique ID

### LED Control Not Working

1. **Check LED Pin**: Verify the LED pin number is correct for your board
2. **Wait for Discovery**: Control only works after boards discover each other
3. **Check Callbacks**: Ensure pin control callbacks are properly registered

## Customizing the Examples

### Changing Communication Pins

For ESP32 Hardware Serial:

```cpp
// Define pins for UART communication
#define RX_PIN 16  // GPIO16 for RX
#define TX_PIN 17  // GPIO17 for TX

// Initialize Serial2 with custom pins
Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
```

For SoftwareSerial:

```cpp
// Define pins for SoftwareSerial communication
#define RX_PIN 10  // RX pin for SoftwareSerial
#define TX_PIN 11  // TX pin for SoftwareSerial

// Create SoftwareSerial instance with custom pins
SoftwareSerial commSerial(RX_PIN, TX_PIN);
```

### Adding More Boards

The PinComm library supports up to 20 peer boards. To add more boards:

1. Assign a unique ID to each board
2. Connect all boards to a common GND
3. For multiple boards, consider using a bus topology or separate UART ports

## Advanced Usage

### Using Hardware Flow Control

For more reliable communication, you can use hardware flow control (RTS/CTS) if your boards support it:

```cpp
// For ESP32 with hardware flow control
Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN, CTS_PIN, RTS_PIN);
```

### Increasing Communication Speed

You can increase the baud rate for faster communication:

```cpp
// Higher baud rate for faster communication
Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
pinComm.begin(&Serial2, boardId);
```

Note: Higher baud rates may require shorter wires and better signal integrity.

## Further Resources

- [PinComm Library Documentation](../../README.md)
- [Arduino Serial Communication Guide](https://www.arduino.cc/reference/en/language/functions/communication/serial/)
- [ESP32 UART Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/uart.html)
