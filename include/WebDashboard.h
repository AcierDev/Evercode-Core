/**
 * WebDashboard.h - Lightweight web dashboard for ESP32
 * Created by Claude, 2023
 *
 * A minimal, efficient web dashboard for ESP32 devices with a focus on:
 * - Dense, information-rich UI
 * - Low overhead and memory usage
 * - Simple API for C++ beginners
 * - Fast loading and responsive design
 */

#ifndef WebDashboard_h
#define WebDashboard_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// Maximum number of components and clients
#define MAX_DASHBOARD_COMPONENTS 50
#define MAX_DASHBOARD_CLIENTS 5
#define MAX_LOG_ENTRIES 100
#define MAX_COMPONENT_ID_LENGTH 32
#define MAX_LOG_LENGTH 256
#define MAX_LOG_RETENTION_TIME 1000 * 60 * 10  // 10 minutes

// Timeouts and intervals
#define DASHBOARD_UPDATE_INTERVAL 500  // Update interval in ms
#define CLIENT_TIMEOUT 30000           // Client timeout in ms

// Log levels
#define LOG_INFO 0
#define LOG_WARNING 1
#define LOG_ERROR 2
#define LOG_DEBUG 3

// Component types
enum class ComponentType {
  BUTTON = 1,
  TOGGLE = 2,
  SLIDER = 3,
  TEXT_INPUT = 4,
  SELECT = 5,
  PIN_MONITOR = 6,
  MACHINE_STATE = 7
};

// Callback function types
typedef void (*ButtonCallback)(const char* id);
typedef void (*ToggleCallback)(const char* id, bool state);
typedef void (*SliderCallback)(const char* id, int value);
typedef void (*TextInputCallback)(const char* id, const char* value);
typedef void (*SelectCallback)(const char* id, const char* value);
typedef void (*StateChangeCallback)(const char* oldState, const char* newState);
typedef void (*WebClientConnectCallback)(const char* clientIp);

class WebDashboard {
 public:
  // Component tracking
  struct DashboardComponent {
    char id[MAX_COMPONENT_ID_LENGTH];
    ComponentType type;
    bool active;
    void* callback;
    char label[64];
    union {
      struct {
        int min;
        int max;
        int step;
      } slider;
      struct {
        char** options;
        int optionCount;
      } select;
      struct {
        uint8_t pin;
        uint8_t mode;
        uint32_t updateInterval;
        uint32_t lastUpdate;
        bool isAnalog;
      } pinMonitor;
    } config;
    DynamicJsonDocument* data;
  };

  /**
   * Constructor for WebDashboard
   */
  WebDashboard();

  /**
   * Initialize the web dashboard
   *
   * @param ssid WiFi network SSID to connect to
   * @param password WiFi network password
   * @param title Dashboard title (shown in browser)
   * @param port Web server port (default: 80)
   * @return true if initialization was successful
   */
  bool begin(const char* ssid, const char* password, const char* title,
             int port = 80);

  /**
   * Main loop function that must be called regularly
   */
  void update();

  /**
   * Get the IP address of the dashboard as a string
   */
  String getIPAddress();

  /**
   * Set the machine state displayed in the header
   */
  void setMachineState(const char* state);

  /**
   * Get the current machine state
   */
  const char* getMachineState();

  /**
   * Register a callback for machine state changes
   */
  void onStateChange(StateChangeCallback callback);

  // ==================== Controls API ====================

  /**
   * Add a button to the Controls page
   *
   * @param id Unique identifier for the button
   * @param label Text to display on the button
   * @param callback Function to call when button is pressed
   * @return true if successful
   */
  bool addButton(const char* id, const char* label, ButtonCallback callback);

  // ==================== Settings API ====================

  /**
   * Add a toggle setting
   *
   * @param id Unique identifier for the toggle
   * @param label Text to display next to the toggle
   * @param initialState Initial state (true = on, false = off)
   * @param callback Function to call when toggle changes
   * @return true if successful
   */
  bool addToggle(const char* id, const char* label, bool initialState,
                 ToggleCallback callback);

  /**
   * Add a slider setting
   *
   * @param id Unique identifier for the slider
   * @param label Text to display next to the slider
   * @param min Minimum value
   * @param max Maximum value
   * @param step Step size (default: 1)
   * @param initialValue Initial value
   * @param callback Function to call when slider changes
   * @return true if successful
   */
  bool addSlider(const char* id, const char* label, int min, int max,
                 int initialValue, int step = 1,
                 SliderCallback callback = NULL);

  /**
   * Add a text input setting
   *
   * @param id Unique identifier for the input
   * @param label Text to display next to the input
   * @param initialValue Initial value
   * @param callback Function to call when input changes
   * @return true if successful
   */
  bool addTextInput(const char* id, const char* label, const char* initialValue,
                    TextInputCallback callback = NULL);

  /**
   * Add a select/dropdown setting
   *
   * @param id Unique identifier for the select
   * @param label Text to display next to the select
   * @param options Array of option strings
   * @param optionCount Number of options
   * @param initialValue Initial selected option
   * @param callback Function to call when selection changes
   * @return true if successful
   */
  bool addSelect(const char* id, const char* label, const char** options,
                 int optionCount, const char* initialValue,
                 SelectCallback callback = NULL);

  // ==================== Monitoring API ====================

  /**
   * Add a pin monitor
   *
   * @param id Unique identifier for the pin monitor
   * @param label Text to display next to the monitor
   * @param pin Pin number to monitor
   * @param mode Pin mode (INPUT, INPUT_PULLUP, etc.)
   * @param isAnalog Whether to read as analog or digital
   * @param updateInterval How often to update (in ms)
   * @return true if successful
   */
  bool addPinMonitor(const char* id, const char* label, uint8_t pin,
                     uint8_t mode, bool isAnalog = false,
                     uint32_t updateInterval = 100);

  /**
   * Log a message to the dashboard
   *
   * @param message Message text
   * @param level Log level (0=info, 1=warning, 2=error, 3=debug)
   * @return true if successful
   */
  bool log(const char* message, uint8_t level = LOG_INFO);

  /**
   * Log a formatted message (printf style)
   *
   * @param level Log level (0=info, 1=warning, 2=error, 3=debug)
   * @param format Format string (printf style)
   * @param ... Variable arguments for format string
   * @return true if successful
   */
  bool logf(uint8_t level, const char* format, ...);

  /**
   * Update a component's value
   *
   * @param id Unique identifier of the component
   * @param value New value (string)
   * @return true if successful
   */
  bool updateValue(const char* id, const char* value);

  /**
   * Update a component's value (integer version)
   *
   * @param id Unique identifier of the component
   * @param value New value (integer)
   * @return true if successful
   */
  bool updateValue(const char* id, int value);

  /**
   * Update a component's value (float version)
   *
   * @param id Unique identifier of the component
   * @param value New value (float)
   * @param precision Number of decimal places
   * @return true if successful
   */
  bool updateValue(const char* id, float value, int precision = 2);

  /**
   * Update a component's value (boolean version)
   *
   * @param id Unique identifier of the component
   * @param value New value (boolean)
   * @return true if successful
   */
  bool updateValue(const char* id, bool value);

  /**
   * Check if the dashboard is online
   */
  bool isOnline();

  /**
   * Enable debug logging
   */
  bool enableDebugLogging(bool enable);

  /**
   * Get the debug logging enabled state
   */
  bool isDebugLoggingEnabled();

 private:
  bool _isInitialized;
  bool _debugLoggingEnabled;
  char _dashboardTitle[64];
  char _machineState[64];
  uint32_t _lastUpdate;
  IPAddress _ipAddress;
  StateChangeCallback _stateChangeCallback;

  // Web server
  AsyncWebServer* _server;
  AsyncWebSocket* _ws;

  // Component tracking
  DashboardComponent _components[MAX_DASHBOARD_COMPONENTS];
  int _componentCount;

  // Client tracking
  struct WebClient {
    uint32_t id;
    IPAddress ip;
    uint32_t lastSeen;
    bool active;
  };

  WebClient _clients[MAX_DASHBOARD_CLIENTS];
  int _clientCount;
  WebClientConnectCallback _clientConnectCallback;

  // Log storage
  struct LogEntry {
    char message[MAX_LOG_LENGTH];
    uint8_t level;
    uint32_t timestamp;
    bool active;
  };

  LogEntry _logEntries[MAX_LOG_ENTRIES];
  int _logEntryCount;
  int _logEntryIndex;

  // Internal helper methods
  void handleWebSocketEvent(AsyncWebSocket* server,
                            AsyncWebSocketClient* client, AwsEventType type,
                            void* arg, uint8_t* data, size_t len);
  void processWebSocketMessage(uint32_t clientId, const char* message);
  void broadcastDashboardUpdate(bool fullUpdate = false);
  void broadcastComponentUpdate(const char* componentId);
  void cleanupOldLogs();
  void handleNotFound(AsyncWebServerRequest* request);
  DashboardComponent* findComponent(const char* id);
};

#endif  // WebDashboard_h