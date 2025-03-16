/**
 * WebDashboard Pin Monitoring Example
 *
 * This example demonstrates how to use the WebDashboard library to monitor
 * both digital and analog pins on an ESP32.
 *
 * Hardware setup:
 * - Connect a button to pin 5 (with pull-up resistor)
 * - Connect a potentiometer to pin 34 (ADC)
 * - Connect an LED to pin 2 (or use the built-in LED)
 *
 * Instructions:
 * 1. Modify the WiFi credentials below to match your network
 * 2. Upload this sketch to your ESP32
 * 3. Open the Serial Monitor to see the IP address
 * 4. Open a web browser and navigate to the displayed IP address
 * 5. Interact with the pins to see the values change in real-time
 */

#include <WebDashboard.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Pin definitions
const int BUTTON_PIN = 5;
const int POTENTIOMETER_PIN = 34;
const int LED_PIN = 2;  // Built-in LED on most ESP32 boards

// Dashboard instance
WebDashboard dashboard;

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(100);

  Serial.println("\nInitializing WebDashboard Pin Monitoring Example...");

  // Initialize pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(POTENTIOMETER_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  // Start the dashboard
  if (dashboard.begin(ssid, password, "ESP32 Pin Monitor")) {
    Serial.println("Dashboard started!");
    Serial.print("Dashboard available at http://");
    Serial.println(dashboard.getIPAddress());
  } else {
    Serial.println("Failed to start dashboard. Check WiFi connection.");
    while (1) delay(1000);
  }

  // Enable debug logging
  dashboard.enableDebugLogging(true);

  // Register digital pin monitor (button)
  dashboard.registerPinMonitor("button", "Button State (Digital)", BUTTON_PIN,
                               INPUT_PULLUP, false, 100);

  // Register analog pin monitor (potentiometer)
  dashboard.registerPinMonitor("pot", "Potentiometer Value (Analog)",
                               POTENTIOMETER_PIN, INPUT, true, 200);

  // Register status display for pin information
  dashboard.registerStatus("info", "Pin Information",
                           "Button on GPIO5, Potentiometer on GPIO34");

  // Register button to toggle LED
  dashboard.registerButton("toggle_led", "Toggle LED", onToggleLED);

  // Add a log display
  dashboard.registerLogDisplay("log", "System Logs", 10);

  // Log initial message
  dashboard.log(
      "Pin monitoring started. Try pressing the button or turning the "
      "potentiometer.");
}

void loop() {
  // Update the dashboard
  dashboard.update();

  // Read button state and update LED accordingly
  static int lastButtonState = -1;
  int buttonState = digitalRead(BUTTON_PIN);

  // Log button press events (when state changes from HIGH to LOW)
  if (buttonState == LOW && lastButtonState == HIGH) {
    dashboard.log("Button pressed!");
  }

  lastButtonState = buttonState;

  // Small delay to prevent excessive processing
  delay(10);
}

// Callback function for the toggle LED button
void onToggleLED(const char* buttonId) {
  static bool ledState = false;
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);

  // Log the action
  dashboard.logf(0, "LED toggled to %s", ledState ? "ON" : "OFF");
}