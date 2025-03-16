/**
 * WebDashboard.h - Library for web-based dashboard functionality
 * Created by Claude, 2023
 *
 * This library provides a web dashboard interface for ESP32 devices,
 * allowing monitoring and control from a browser. It integrates with
 * existing components like NetworkComm and supports UI elements that
 * can automatically report their state.
 *
 * IMPORTANT USAGE NOTES:
 * - Requires WiFi connectivity (configure with begin())
 * - Optimized for ESP32 with minimal resource usage
 * - Supports real-time updates via WebSockets
 * - Can integrate with Bounce2 buttons and other components
 */

#ifndef WebDashboard_h
#define WebDashboard_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>

// Dashboard component types
enum class DashCompType {
  BUTTON = 1,
  SWITCH = 2,
  SLIDER = 3,
  GAUGE = 4,
  CHART = 5,
  TEXT = 6,
  LOG = 7,
  ALERT = 8,
  STATE = 9,
  STATUS = 10
};

// Dashboard event types
#define DASH_EVENT_STATE_CHANGE 1
#define DASH_EVENT_BUTTON_PRESS 2
#define DASH_EVENT_SLIDER_CHANGE 3
#define DASH_EVENT_SWITCH_TOGGLE 4
#define DASH_EVENT_COMPONENT_UPDATE 5
#define DASH_EVENT_ALERT 6
#define DASH_EVENT_LOG 7

// Maximum number of components and clients
#define MAX_DASHBOARD_COMPONENTS 50
#define MAX_DASHBOARD_CLIENTS 10
#define MAX_STATE_MACHINES 5
#define MAX_LOG_ENTRIES 100
#define MAX_COMPONENT_ID_LENGTH 32
#define MAX_STATE_NAME_LENGTH 32
#define MAX_ALERT_LENGTH 256
#define MAX_LOG_LENGTH 256

// Timeouts and intervals
#define DASHBOARD_UPDATE_INTERVAL 500  // Update interval in ms
#define CLIENT_TIMEOUT 30000           // Client timeout in ms
#define LOG_RETENTION_TIME 3600000     // Log retention time (1 hour)

// Callback function types
typedef void (*ComponentUpdateCallback)(const char* componentId,
                                        const char* value);
typedef void (*ButtonPressCallback)(const char* buttonId);
typedef void (*StateChangeCallback)(const char* machine, const char* oldState,
                                    const char* newState);
typedef void (*SliderChangeCallback)(const char* sliderId, int value);
typedef void (*SwitchToggleCallback)(const char* switchId, bool state);
typedef void (*WebClientConnectCallback)(const char* clientIp);

class WebDashboard {
 public:
  // Component tracking
  struct DashboardComponent {
    char id[MAX_COMPONENT_ID_LENGTH];
    DashCompType type;
    bool active;
    void* callback;
    char label[64];
    union {
      struct {
        int min;
        int max;
        char units[16];
      } gauge;
      struct {
        int min;
        int max;
        char xLabel[32];
        char yLabel[32];
        int maxPoints;
      } chart;
      struct {
        char** states;
        int stateCount;
        char currentState[MAX_STATE_NAME_LENGTH];
      } stateMachine;
      struct {
        int maxEntries;
      } logDisplay;
    } config;
    DynamicJsonDocument* data;
  };

  /**
   * Constructor for WebDashboard
   *
   * Initializes internal variables but does not start the dashboard.
   * Call begin() to start the web dashboard functionality.
   */
  WebDashboard();

  // ==================== Initialization ====================
  /**
   * Initialize the web dashboard
   *
   * @param ssid WiFi network SSID to connect to
   * @param password WiFi network password
   * @param title Dashboard title (shown in browser)
   * @param port Web server port (default: 80)
   * @return true if initialization was successful, false otherwise
   */
  bool begin(const char* ssid, const char* password, const char* title,
             int port = 80);

  /**
   * Main loop function that must be called regularly
   *
   * This function handles updates, timeouts, and periodic tasks.
   * It should be called in the Arduino loop().
   */
  void update();

  /**
   * Check if the dashboard is connected and serving web pages
   *
   * @return true if the dashboard is online and operational
   */
  bool isOnline();

  /**
   * Get the IP address of the dashboard as a string
   *
   * @return String containing the current IP address (e.g., "192.168.1.100")
   */
  String getIPAddress();

  /**
   * Enable or disable debug logging for dashboard events
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

  // ==================== Component Management ====================
  /**
   * Register a dashboard button component
   *
   * @param id Unique identifier for the button
   * @param label Text to display on the button
   * @param callback Function to call when the button is pressed
   * @return true if the component was registered successfully
   */
  bool registerButton(const char* id, const char* label,
                      ButtonPressCallback callback);

  /**
   * Register a dashboard switch component
   *
   * @param id Unique identifier for the switch
   * @param label Text to display next to the switch
   * @param initialState Initial state of the switch (true = on, false = off)
   * @param callback Function to call when the switch is toggled
   * @return true if the component was registered successfully
   */
  bool registerSwitch(const char* id, const char* label, bool initialState,
                      SwitchToggleCallback callback);

  /**
   * Register a dashboard slider component
   *
   * @param id Unique identifier for the slider
   * @param label Text to display next to the slider
   * @param min Minimum value of the slider
   * @param max Maximum value of the slider
   * @param initialValue Initial value of the slider
   * @param callback Function to call when the slider value changes
   * @return true if the component was registered successfully
   */
  bool registerSlider(const char* id, const char* label, int min, int max,
                      int initialValue, SliderChangeCallback callback);

  /**
   * Register a dashboard gauge component
   *
   * @param id Unique identifier for the gauge
   * @param label Text to display next to the gauge
   * @param min Minimum value of the gauge
   * @param max Maximum value of the gauge
   * @param initialValue Initial value of the gauge
   * @param units Units to display for the gauge value (e.g., "°C", "rpm")
   * @return true if the component was registered successfully
   */
  bool registerGauge(const char* id, const char* label, int min, int max,
                     int initialValue, const char* units);

  /**
   * Register a dashboard text component
   *
   * @param id Unique identifier for the text component
   * @param label Text to display next to the component
   * @param initialValue Initial text value
   * @return true if the component was registered successfully
   */
  bool registerText(const char* id, const char* label,
                    const char* initialValue);

  /**
   * Register a dashboard status display component
   *
   * @param id Unique identifier for the status component
   * @param label Text to display next to the status
   * @param initialValue Initial status text
   * @return true if the component was registered successfully
   */
  bool registerStatus(const char* id, const char* label,
                      const char* initialValue);

  /**
   * Register a dashboard chart component
   *
   * @param id Unique identifier for the chart
   * @param title Title to display above the chart
   * @param xLabel Label for the X axis
   * @param yLabel Label for the Y axis
   * @param maxDataPoints Maximum number of data points to keep in history
   * @return true if the component was registered successfully
   */
  bool registerChart(const char* id, const char* title, const char* xLabel,
                     const char* yLabel, int maxDataPoints = 50);

  /**
   * Register a dashboard log display component
   *
   * @param id Unique identifier for the log component
   * @param title Title to display above the log
   * @param maxLogEntries Maximum number of log entries to display
   * @return true if the component was registered successfully
   */
  bool registerLogDisplay(const char* id, const char* title,
                          int maxLogEntries = 20);

  /**
   * Register a dashboard alert display component
   *
   * @param id Unique identifier for the alert component
   * @param title Title to display above the alerts
   * @return true if the component was registered successfully
   */
  bool registerAlertDisplay(const char* id, const char* title);

  /**
   * Register a dashboard state machine display component
   *
   * @param id Unique identifier for the state machine component
   * @param title Title to display above the state machine
   * @param states Array of possible state names
   * @param stateCount Number of possible states
   * @param initialState Initial state name
   * @param callback Function to call when state changes from UI
   * @return true if the component was registered successfully
   */
  bool registerStateMachine(const char* id, const char* title,
                            const char** states, int stateCount,
                            const char* initialState,
                            StateChangeCallback callback = NULL);

  /**
   * Unregister a dashboard component
   *
   * @param id Unique identifier of the component to remove
   * @return true if the component was unregistered successfully
   */
  bool unregisterComponent(const char* id);

  // ==================== Component Updates ====================
  /**
   * Update a dashboard component's value
   *
   * @param id Unique identifier of the component
   * @param value New value for the component
   * @return true if the component was updated successfully
   */
  bool updateComponent(const char* id, const char* value);

  /**
   * Update a dashboard component's value (integer version)
   *
   * @param id Unique identifier of the component
   * @param value New value for the component
   * @return true if the component was updated successfully
   */
  bool updateComponent(const char* id, int value);

  /**
   * Update a dashboard component's value (float version)
   *
   * @param id Unique identifier of the component
   * @param value New value for the component
   * @param precision Number of decimal places to include
   * @return true if the component was updated successfully
   */
  bool updateComponent(const char* id, float value, int precision = 2);

  /**
   * Update a dashboard component's value (boolean version)
   *
   * @param id Unique identifier of the component
   * @param value New value for the component
   * @return true if the component was updated successfully
   */
  bool updateComponent(const char* id, bool value);

  /**
   * Add a data point to a chart component
   *
   * @param id Unique identifier of the chart component
   * @param x X value for the data point
   * @param y Y value for the data point
   * @return true if the data point was added successfully
   */
  bool addChartDataPoint(const char* id, float x, float y);

  /**
   * Add a data point to a chart component with timestamp
   *
   * @param id Unique identifier of the chart component
   * @param y Y value for the data point
   * @return true if the data point was added successfully
   * @note The X value will be automatically set to the current time
   */
  bool addChartDataPoint(const char* id, float y);

  /**
   * Clear all data points from a chart component
   *
   * @param id Unique identifier of the chart component
   * @return true if the chart was cleared successfully
   */
  bool clearChartData(const char* id);

  // ==================== State Machine Management ====================
  /**
   * Update the state of a state machine component
   *
   * @param id Unique identifier of the state machine component
   * @param state New state name
   * @return true if the state was updated successfully
   */
  bool updateState(const char* id, const char* state);

  /**
   * Get the current state of a state machine component
   *
   * @param id Unique identifier of the state machine component
   * @return String containing the current state name
   */
  String getCurrentState(const char* id);

  /**
   * Register a callback for when a state changes
   *
   * @param id Unique identifier of the state machine component
   * @param callback Function to call when the state changes
   * @return true if the callback was registered successfully
   */
  bool onStateChange(const char* id, StateChangeCallback callback);

  // ==================== Logging & Alerts ====================
  /**
   * Add a log entry to the dashboard log
   *
   * @param message Log message text
   * @param level Log level (0=info, 1=warning, 2=error, 3=debug)
   * @return true if the log entry was added successfully
   */
  bool log(const char* message, uint8_t level = 0);

  /**
   * Add a log entry with formatting (printf style)
   *
   * @param level Log level (0=info, 1=warning, 2=error, 3=debug)
   * @param format Format string (printf style)
   * @param ... Variable arguments for format string
   * @return true if the log entry was added successfully
   */
  bool logf(uint8_t level, const char* format, ...);

  /**
   * Send an alert to the dashboard
   *
   * @param message Alert message text
   * @param level Alert level (0=info, 1=warning, 2=error)
   * @return true if the alert was sent successfully
   */
  bool alert(const char* message, uint8_t level = 1);

  /**
   * Send an alert with formatting (printf style)
   *
   * @param level Alert level (0=info, 1=warning, 2=error)
   * @param format Format string (printf style)
   * @param ... Variable arguments for format string
   * @return true if the alert was sent successfully
   */
  bool alertf(uint8_t level, const char* format, ...);

  /**
   * Clear all alerts from the dashboard
   *
   * @return true if alerts were cleared successfully
   */
  bool clearAlerts();

  // ==================== Client Management ====================
  /**
   * Get the number of currently connected clients
   *
   * @return Number of connected web clients
   */
  int getConnectedClientCount();

  /**
   * Register a callback for when a new client connects
   *
   * @param callback Function to call when a new client connects
   * @return true if the callback was registered successfully
   */
  bool onClientConnect(WebClientConnectCallback callback);

  /**
   * Set authentication credentials for the dashboard
   *
   * @param username Username for authentication
   * @param password Password for authentication
   * @return true if authentication was set successfully
   */
  bool setAuthentication(const char* username, const char* password);

  /**
   * Check if authentication is enabled
   *
   * @return true if authentication is enabled, false otherwise
   */
  bool isAuthenticationEnabled();

  /**
   * Enable or disable cross-origin resource sharing (CORS)
   *
   * @param enable true to enable CORS, false to disable
   * @return true if the setting was applied successfully
   */
  bool enableCORS(bool enable);

  /**
   * Find a dashboard component by ID
   *
   * @param id The unique identifier of the component to find
   * @return Pointer to the component, or NULL if not found
   * @note This is made public to allow external code to check component
   * existence
   */
  DashboardComponent* findComponent(const char* id);

  /**
   * Check if a component's value matches the expected value
   *
   * @param id Unique identifier of the component
   * @param expectedValue Value to check against
   * @return true if the component exists and its value matches expectedValue
   */
  bool isComponentValue(const char* id, bool expectedValue);

  /**
   * Check if a component's value matches the expected value (string version)
   *
   * @param id Unique identifier of the component
   * @param expectedValue Value to check against
   * @return true if the component exists and its value matches expectedValue
   */
  bool isComponentValue(const char* id, const char* expectedValue);

  /**
   * Check if a component's value matches the expected value (integer version)
   *
   * @param id Unique identifier of the component
   * @param expectedValue Value to check against
   * @return true if the component exists and its value matches expectedValue
   */
  bool isComponentValue(const char* id, int expectedValue);

 private:
  // Internal structures and state
  bool _isInitialized;
  bool _debugLoggingEnabled;
  char _dashboardTitle[64];
  uint32_t _lastUpdate;
  bool _authEnabled;
  char _authUsername[32];
  char _authPassword[32];
  bool _corsEnabled;
  IPAddress _ipAddress;

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
  void initialize();
  bool setupWebServer();
  void addCORS(AsyncWebServerResponse* response);
  void handleNotFound(AsyncWebServerRequest* request);
  void handleAuthentication(AsyncWebServerRequest* request);
};

#endif  // WebDashboard_h