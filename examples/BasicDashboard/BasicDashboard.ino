/**
 * BasicDashboard.ino - Example for the WebDashboard library
 *
 * This example demonstrates the basic functionality of the WebDashboard
 * library:
 * - Setting up a dashboard with a machine state
 * - Adding controls (buttons)
 * - Adding settings (toggle, slider, text input, select)
 * - Adding pin monitoring
 * - Logging messages and alerts
 *
 * Connect to the dashboard using a web browser at the IP address shown in the
 * serial monitor.
 */

#include <WebDashboard.h>

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// Create dashboard instance
WebDashboard dashboard;

// Pin definitions
const int LED_PIN = 2;      // Built-in LED on most ESP32 boards
const int BUTTON_PIN = 0;   // Boot button on most ESP32 boards
const int ANALOG_PIN = 34;  // Analog input pin (ADC)

// Variables for demo
bool ledState = false;
int sliderValue = 50;
char textValue[64] = "Hello World";
const char* options[] = {"Option 1", "Option 2", "Option 3", "Option 4"};
const char* selectedOption = "Option 1";
unsigned long lastStateChange = 0;
int stateIndex = 0;
const char* states[] = {"IDLE", "RUNNING", "PAUSED", "ERROR"};

void setup() {
  // Initialize serial
  Serial.begin(115200);
  Serial.println("WebDashboard Basic Example");

  // Set pin modes
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize dashboard
  if (dashboard.begin(ssid, password, "ESP32 Dashboard")) {
    Serial.print("Dashboard started at http://");
    Serial.println(dashboard.getIPAddress());
  } else {
    Serial.println("Failed to start dashboard");
    while (1) delay(100);
  }

  // Set initial machine state
  dashboard.setMachineState("IDLE");
  dashboard.onStateChange(onMachineStateChange);

  // Add controls
  dashboard.addButton("btn_led_toggle", "Toggle LED", onLedToggle);
  dashboard.addButton("btn_log_info", "Log Info", onLogInfo);
  dashboard.addButton("btn_log_warning", "Log Warning", onLogWarning);
  dashboard.addButton("btn_log_error", "Log Error", onLogError);
  dashboard.addButton("btn_next_state", "Next State", onNextState);

  // Add settings
  dashboard.addToggle("toggle_led", "LED Control", ledState,
                      onLedToggleFromSettings);
  dashboard.addSlider("slider_brightness", "Brightness", 0, 255, sliderValue, 1,
                      onSliderChange);
  dashboard.addTextInput("text_message", "Message", textValue, onTextChange);
  dashboard.addSelect("select_option", "Options", options, 4, selectedOption,
                      onSelectChange);

  // Add pin monitoring
  dashboard.addPinMonitor("pin_led", "LED Pin", LED_PIN, OUTPUT, false, 100);
  dashboard.addPinMonitor("pin_button", "Button Pin", BUTTON_PIN, INPUT_PULLUP,
                          false, 100);
  dashboard.addPinMonitor("pin_analog", "Analog Pin", ANALOG_PIN, INPUT, true,
                          500);

  // Log startup message
  dashboard.log("Dashboard started successfully");
}

void loop() {
  // Update dashboard
  dashboard.update();

  // Read button state and update LED if pressed
  static bool lastButtonState = HIGH;
  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && lastButtonState == HIGH) {
    // Button pressed
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    dashboard.updateValue("toggle_led", ledState);
    dashboard.logf(LOG_INFO, "Button pressed, LED is now %s",
                   ledState ? "ON" : "OFF");
  }
  lastButtonState = buttonState;

  // Change state every 10 seconds for demo
  if (millis() - lastStateChange > 10000) {
    lastStateChange = millis();
    stateIndex = (stateIndex + 1) % 4;
    dashboard.setMachineState(states[stateIndex]);

    // Log state change
    dashboard.logf(LOG_INFO, "Machine state changed to %s", states[stateIndex]);

    // If state is ERROR, send an alert
    if (strcmp(states[stateIndex], "ERROR") == 0) {
      dashboard.alert("System entered ERROR state!");
    }
  }

  // Small delay to prevent CPU hogging
  delay(10);
}

// Button callbacks
void onLedToggle(const char* id) {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState);
  dashboard.updateValue("toggle_led", ledState);
  dashboard.logf(LOG_INFO, "LED toggled from button, now %s",
                 ledState ? "ON" : "OFF");
}

void onLogInfo(const char* id) {
  dashboard.log("This is an info message", LOG_INFO);
}

void onLogWarning(const char* id) {
  dashboard.log("This is a warning message", LOG_WARNING);
}

void onLogError(const char* id) { dashboard.alert("This is an error message"); }

void onNextState(const char* id) {
  stateIndex = (stateIndex + 1) % 4;
  dashboard.setMachineState(states[stateIndex]);
  dashboard.logf(LOG_INFO, "Machine state manually changed to %s",
                 states[stateIndex]);
}

// Settings callbacks
void onLedToggleFromSettings(const char* id, bool state) {
  ledState = state;
  digitalWrite(LED_PIN, ledState);
  dashboard.logf(LOG_INFO, "LED toggled from settings, now %s",
                 ledState ? "ON" : "OFF");
}

void onSliderChange(const char* id, int value) {
  sliderValue = value;

  // If we had PWM, we could set LED brightness
  // analogWrite(LED_PIN, value);

  dashboard.logf(LOG_INFO, "Brightness set to %d", value);
}

void onTextChange(const char* id, const char* value) {
  strncpy(textValue, value, sizeof(textValue) - 1);
  dashboard.logf(LOG_INFO, "Message changed to: %s", value);
}

void onSelectChange(const char* id, const char* value) {
  selectedOption = value;
  dashboard.logf(LOG_INFO, "Option changed to: %s", value);
}

// Machine state change callback
void onMachineStateChange(const char* oldState, const char* newState) {
  Serial.printf("Machine state changed from %s to %s\n", oldState, newState);
}