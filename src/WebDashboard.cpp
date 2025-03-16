#include "../include/WebDashboard.h"

#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include <stdarg.h>

#include "../include/DashboardHTML.h"

// Add converter for ComponentType
namespace ARDUINOJSON_NAMESPACE {
template <>
struct Converter<ComponentType> {
  static void toJson(ComponentType src, JsonVariant dst) {
    dst.set(static_cast<int>(src));
  }
  static ComponentType fromJson(JsonVariantConst src) {
    return static_cast<ComponentType>(src.as<int>());
  }
};
}  // namespace ARDUINOJSON_NAMESPACE

// Constructor
WebDashboard::WebDashboard() {
  _isInitialized = false;
  _debugLoggingEnabled = false;
  _lastUpdate = 0;
  _componentCount = 0;
  _clientCount = 0;
  _logEntryCount = 0;
  _logEntryIndex = 0;
  _clientConnectCallback = NULL;
  memset(_dashboardTitle, 0, sizeof(_dashboardTitle));
  memset(_machineState, 0, sizeof(_machineState));
  strcpy(_machineState, "UNKNOWN");
}

// Initialization and setup
bool WebDashboard::begin(const char* ssid, const char* password,
                         const char* title, int port) {
  if (_isInitialized) {
    return false;
  }

  // Save dashboard title
  strncpy(_dashboardTitle, title, sizeof(_dashboardTitle) - 1);

  // Initialize Wi-Fi
  WiFi.begin(ssid, password);

  // Wait for connection with timeout
  uint32_t startTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    if (millis() - startTime > 20000) {  // 20 second timeout
      if (_debugLoggingEnabled) {
        Serial.println("Failed to connect to WiFi");
      }
      return false;
    }
  }

  _ipAddress = WiFi.localIP();

  if (_debugLoggingEnabled) {
    Serial.print("Connected to WiFi. IP address: ");
    Serial.println(_ipAddress);
  }

  // Initialize server and WebSocket
  _server = new AsyncWebServer(port);
  _ws = new AsyncWebSocket("/ws");

  if (!_server || !_ws) {
    if (_debugLoggingEnabled) {
      Serial.println("Failed to create server or WebSocket");
    }
    return false;
  }

  // Set up WebSocket event handler
  _ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    this->handleWebSocketEvent(server, client, type, arg, data, len);
  });

  _server->addHandler(_ws);

  // Define web routes
  _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    // Replace placeholders in the HTML template
    String html = FPSTR(DASHBOARD_HTML);
    html.replace("%DASHBOARD_TITLE%", _dashboardTitle);

    AsyncWebServerResponse* response =
        request->beginResponse(200, "text/html", html);

    request->send(response);
  });

  // Handle 404 (Page Not Found) errors
  _server->onNotFound([this](AsyncWebServerRequest* request) {
    this->handleNotFound(request);
  });

  // Start server
  _server->begin();

  _isInitialized = true;

  if (_debugLoggingEnabled) {
    Serial.printf("Dashboard started at http://%s:%d\n",
                  _ipAddress.toString().c_str(), port);
  }

  return true;
}

void WebDashboard::update() {
  if (!_isInitialized) {
    return;
  }

  // Update only every DASHBOARD_UPDATE_INTERVAL ms
  if (millis() - _lastUpdate < DASHBOARD_UPDATE_INTERVAL) {
    return;
  }

  _lastUpdate = millis();

  // Clean up inactive clients
  for (int i = 0; i < _clientCount; i++) {
    if (_clients[i].active &&
        (millis() - _clients[i].lastSeen > CLIENT_TIMEOUT)) {
      _clients[i].active = false;
      if (_debugLoggingEnabled) {
        Serial.printf("Client %u timed out\n", _clients[i].id);
      }
    }
  }

  // Clean up old logs
  cleanupOldLogs();

  // Update pin monitors
  uint32_t currentTime = millis();
  for (int i = 0; i < _componentCount; i++) {
    if (_components[i].active &&
        _components[i].type == ComponentType::PIN_MONITOR) {
      // Check if it's time to update this pin
      if (currentTime - _components[i].config.pinMonitor.lastUpdate >=
          _components[i].config.pinMonitor.updateInterval) {
        uint8_t pin = _components[i].config.pinMonitor.pin;
        bool isAnalog = _components[i].config.pinMonitor.isAnalog;

        // Read the pin value
        int value = isAnalog ? analogRead(pin) : digitalRead(pin);

        // Update if value has changed
        if (_components[i].data &&
            (*_components[i].data)["value"].as<int>() != value) {
          (*_components[i].data)["value"] = value;
          broadcastComponentUpdate(_components[i].id);
        }

        // Update last update time
        _components[i].config.pinMonitor.lastUpdate = currentTime;
      }
    }
  }

  // Broadcast any pending updates
  _ws->cleanupClients();
}

// Dashboard status methods
bool WebDashboard::isOnline() {
  return _isInitialized && (WiFi.status() == WL_CONNECTED);
}

String WebDashboard::getIPAddress() { return _ipAddress.toString(); }

bool WebDashboard::enableDebugLogging(bool enable) {
  _debugLoggingEnabled = enable;
  return true;
}

bool WebDashboard::isDebugLoggingEnabled() { return _debugLoggingEnabled; }

// Component management methods
bool WebDashboard::addButton(const char* id, const char* label,
                             ButtonCallback callback) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = ComponentType::BUTTON;
  comp->active = true;
  comp->callback = (void*)callback;
  comp->data = NULL;

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::addToggle(const char* id, const char* label,
                             bool initialState, ToggleCallback callback) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = ComponentType::TOGGLE;
  comp->active = true;
  comp->callback = (void*)callback;

  comp->data = new DynamicJsonDocument(64);
  if (comp->data) {
    (*comp->data)["value"] = initialState;
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::addSlider(const char* id, const char* label, int min,
                             int max, int initialValue, int step,
                             SliderCallback callback) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = ComponentType::SLIDER;
  comp->active = true;
  comp->callback = (void*)callback;
  comp->config.slider.min = min;
  comp->config.slider.max = max;
  comp->config.slider.step = step;

  comp->data = new DynamicJsonDocument(64);
  if (comp->data) {
    (*comp->data)["value"] = initialValue;
    (*comp->data)["min"] = min;
    (*comp->data)["max"] = max;
    (*comp->data)["step"] = step;
  }

  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::addTextInput(const char* id, const char* label,
                                const char* initialValue,
                                TextInputCallback callback) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = ComponentType::TEXT_INPUT;
  comp->active = true;
  comp->callback = (void*)callback;

  comp->data = new DynamicJsonDocument(256);
  if (comp->data) {
    (*comp->data)["value"] = initialValue;
  }

  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::addSelect(const char* id, const char* label,
                             const char** options, int optionCount,
                             const char* initialValue,
                             SelectCallback callback) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = ComponentType::SELECT;
  comp->active = true;
  comp->callback = (void*)callback;

  // Store options
  comp->config.select.options = new char*[optionCount];
  comp->config.select.optionCount = optionCount;
  for (int i = 0; i < optionCount; i++) {
    comp->config.select.options[i] = new char[64];
    strncpy(comp->config.select.options[i], options[i], 63);
  }

  comp->data = new DynamicJsonDocument(512);
  if (comp->data) {
    (*comp->data)["value"] = initialValue;
    JsonArray optionsArray = (*comp->data).createNestedArray("options");
    for (int i = 0; i < optionCount; i++) {
      optionsArray.add(options[i]);
    }
  }

  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::addPinMonitor(const char* id, const char* label, uint8_t pin,
                                 uint8_t mode, bool isAnalog,
                                 uint32_t updateInterval) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  // Set pin mode
  pinMode(pin, mode);

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = ComponentType::PIN_MONITOR;
  comp->active = true;
  comp->callback = NULL;

  // Set pin monitor configuration
  comp->config.pinMonitor.pin = pin;
  comp->config.pinMonitor.mode = mode;
  comp->config.pinMonitor.updateInterval = updateInterval;
  comp->config.pinMonitor.lastUpdate = 0;
  comp->config.pinMonitor.isAnalog = isAnalog;

  // Initialize JSON document for data
  comp->data = new DynamicJsonDocument(128);
  if (comp->data) {
    // Read initial pin value
    int initialValue = isAnalog ? analogRead(pin) : digitalRead(pin);
    (*comp->data)["value"] = initialValue;
    (*comp->data)["min"] = 0;
    (*comp->data)["max"] =
        isAnalog ? 4095 : 1;  // ESP32 has 12-bit ADC (0-4095)
  }

  broadcastComponentUpdate(id);
  return true;
}

// Component update methods
bool WebDashboard::updateValue(const char* id, const char* value) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data) {
    return false;
  }

  (*comp->data)["value"] = value;
  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::updateValue(const char* id, int value) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data) {
    return false;
  }

  (*comp->data)["value"] = value;
  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::updateValue(const char* id, float value, int precision) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data) {
    return false;
  }

  // Use specified precision for floating point values
  char buffer[16];
  dtostrf(value, 0, precision, buffer);
  (*comp->data)["value"] = buffer;

  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::updateValue(const char* id, bool value) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data) {
    return false;
  }

  (*comp->data)["value"] = value;
  broadcastComponentUpdate(id);
  return true;
}

// State machine methods
void WebDashboard::setMachineState(const char* state) {
  char oldState[64];
  strncpy(oldState, _machineState, sizeof(oldState) - 1);

  strncpy(_machineState, state, sizeof(_machineState) - 1);

  // Broadcast the state change
  DynamicJsonDocument doc(128);
  doc["type"] = "machine_state";
  doc["state"] = state;

  String jsonString;
  serializeJson(doc, jsonString);
  _ws->textAll(jsonString);

  // Call the callback if set
  if (_stateChangeCallback) {
    _stateChangeCallback(oldState, state);
  }
}

const char* WebDashboard::getMachineState() { return _machineState; }

void WebDashboard::onStateChange(StateChangeCallback callback) {
  _stateChangeCallback = callback;
}

// Logging & alerts
bool WebDashboard::log(const char* message, uint8_t level) {
  if (!_isInitialized) {
    return false;
  }

  // Store the log entry
  LogEntry* entry = &_logEntries[_logEntryIndex];
  entry->active = true;
  entry->level = level;
  entry->timestamp = millis();
  strncpy(entry->message, message, MAX_LOG_LENGTH - 1);

  // Update index for next entry (circular buffer)
  _logEntryIndex = (_logEntryIndex + 1) % MAX_LOG_ENTRIES;
  if (_logEntryCount < MAX_LOG_ENTRIES) {
    _logEntryCount++;
  }

  // Send to all log display components
  DynamicJsonDocument doc(512);
  doc["type"] = "log";
  JsonObject entryObj = doc.createNestedObject("entry");
  entryObj["message"] = message;
  entryObj["level"] = level;
  entryObj["timestamp"] = entry->timestamp;

  String jsonString;
  serializeJson(doc, jsonString);
  _ws->textAll(jsonString);

  // Also output to serial if debug logging is enabled
  if (_debugLoggingEnabled) {
    const char* levelStr = level == 0   ? "INFO"
                           : level == 1 ? "WARN"
                           : level == 2 ? "ERROR"
                                        : "DEBUG";
    Serial.printf("[%s] %s\n", levelStr, message);
  }

  return true;
}

bool WebDashboard::logf(uint8_t level, const char* format, ...) {
  char buffer[MAX_LOG_LENGTH];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, MAX_LOG_LENGTH - 1, format, args);
  va_end(args);

  return log(buffer, level);
}

// Private methods
void WebDashboard::handleWebSocketEvent(AsyncWebSocket* server,
                                        AsyncWebSocketClient* client,
                                        AwsEventType type, void* arg,
                                        uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    // New client connected
    uint32_t clientId = client->id();
    IPAddress clientIp = client->remoteIP();

    if (_debugLoggingEnabled) {
      Serial.printf("WebSocket client #%u connected from %s\n", clientId,
                    clientIp.toString().c_str());
    }

    // Add to client list or update existing
    bool clientFound = false;
    for (int i = 0; i < _clientCount; i++) {
      if (_clients[i].id == clientId) {
        _clients[i].active = true;
        _clients[i].lastSeen = millis();
        _clients[i].ip = clientIp;
        clientFound = true;
        break;
      }
    }

    if (!clientFound && _clientCount < MAX_DASHBOARD_CLIENTS) {
      _clients[_clientCount].id = clientId;
      _clients[_clientCount].active = true;
      _clients[_clientCount].lastSeen = millis();
      _clients[_clientCount].ip = clientIp;
      _clientCount++;

      // Call client connect callback if set
      if (_clientConnectCallback) {
        _clientConnectCallback(clientIp.toString().c_str());
      }
    }

  } else if (type == WS_EVT_DISCONNECT) {
    // Client disconnected
    uint32_t clientId = client->id();

    if (_debugLoggingEnabled) {
      Serial.printf("WebSocket client #%u disconnected\n", clientId);
    }

    // Mark as inactive in client list
    for (int i = 0; i < _clientCount; i++) {
      if (_clients[i].id == clientId) {
        _clients[i].active = false;
        break;
      }
    }

  } else if (type == WS_EVT_DATA) {
    // Data received from client
    AwsFrameInfo* info = (AwsFrameInfo*)arg;

    if (info->final && info->index == 0 && info->len == len) {
      // Complete message received
      data[len] = 0;  // Null terminate the data
      processWebSocketMessage(client->id(), (const char*)data);
    }
  }
}

void WebDashboard::processWebSocketMessage(uint32_t clientId,
                                           const char* message) {
  // Update client last seen time
  for (int i = 0; i < _clientCount; i++) {
    if (_clients[i].id == clientId) {
      _clients[i].lastSeen = millis();
      break;
    }
  }

  // Parse the JSON message
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, message);

  if (error) {
    if (_debugLoggingEnabled) {
      Serial.printf("Failed to parse WebSocket message: %s\n", error.c_str());
    }
    return;
  }

  // Process based on message type
  const char* type = doc["type"];

  if (strcmp(type, "request_full_update") == 0) {
    // Client requested a full dashboard update
    broadcastDashboardUpdate(true);

  } else if (strcmp(type, "button_press") == 0) {
    // Button press event
    const char* id = doc["id"];
    DashboardComponent* comp = findComponent(id);

    if (comp && comp->type == ComponentType::BUTTON && comp->callback) {
      ButtonCallback callback = (ButtonCallback)comp->callback;
      callback(id);
    }

  } else if (strcmp(type, "toggle_change") == 0) {
    // Toggle change event
    const char* id = doc["id"];
    bool value = doc["value"];

    DashboardComponent* comp = findComponent(id);

    if (comp && comp->type == ComponentType::TOGGLE && comp->data) {
      // Update internal state
      (*comp->data)["value"] = value;

      // Call callback if set
      if (comp->callback) {
        ToggleCallback callback = (ToggleCallback)comp->callback;
        callback(id, value);
      }

      // Broadcast the update to all clients
      broadcastComponentUpdate(id);
    }

  } else if (strcmp(type, "slider_change") == 0) {
    // Slider change event
    const char* id = doc["id"];
    int value = doc["value"];

    DashboardComponent* comp = findComponent(id);

    if (comp && comp->type == ComponentType::SLIDER && comp->data) {
      // Update internal state
      (*comp->data)["value"] = value;

      // Call callback if set
      if (comp->callback) {
        SliderCallback callback = (SliderCallback)comp->callback;
        callback(id, value);
      }

      // Broadcast the update to all clients
      broadcastComponentUpdate(id);
    }

  } else if (strcmp(type, "text_input_change") == 0) {
    // Text input change event
    const char* id = doc["id"];
    const char* value = doc["value"];

    DashboardComponent* comp = findComponent(id);

    if (comp && comp->type == ComponentType::TEXT_INPUT && comp->data) {
      // Update internal state
      (*comp->data)["value"] = value;

      // Call callback if set
      if (comp->callback) {
        TextInputCallback callback = (TextInputCallback)comp->callback;
        callback(id, value);
      }

      // Broadcast the update to all clients
      broadcastComponentUpdate(id);
    }
  } else if (strcmp(type, "select_change") == 0) {
    // Select change event
    const char* id = doc["id"];
    const char* value = doc["value"];

    DashboardComponent* comp = findComponent(id);

    if (comp && comp->type == ComponentType::SELECT && comp->data) {
      // Update internal state
      (*comp->data)["value"] = value;

      // Call callback if set
      if (comp->callback) {
        SelectCallback callback = (SelectCallback)comp->callback;
        callback(id, value);
      }

      // Broadcast the update to all clients
      broadcastComponentUpdate(id);
    }
  }
}

void WebDashboard::broadcastDashboardUpdate(bool fullUpdate) {
  DynamicJsonDocument doc(8192);  // Large document for full dashboard
  doc["type"] = "full_update";
  doc["machineState"] = _machineState;

  JsonArray componentsArray = doc.createNestedArray("components");

  // Add all active components
  for (int i = 0; i < _componentCount; i++) {
    if (_components[i].active) {
      JsonObject component = componentsArray.createNestedObject();
      component["id"] = _components[i].id;
      component["type"] = _components[i].type;
      component["label"] = _components[i].label;

      // Add component-specific configuration
      switch (_components[i].type) {
        case ComponentType::SLIDER: {
          JsonObject config = component.createNestedObject("config");
          config["min"] = _components[i].config.slider.min;
          config["max"] = _components[i].config.slider.max;
          config["step"] = _components[i].config.slider.step;
          break;
        }

        case ComponentType::SELECT: {
          JsonObject config = component.createNestedObject("config");
          JsonArray options = config.createNestedArray("options");
          for (int j = 0; j < _components[i].config.select.optionCount; j++) {
            options.add(_components[i].config.select.options[j]);
          }
          break;
        }
      }

      // Add component data
      if (_components[i].data) {
        for (JsonPair kv : _components[i].data->as<JsonObject>()) {
          component[kv.key().c_str()] = kv.value();
        }
      }
    }
  }

  // Add recent logs
  if (fullUpdate && _logEntryCount > 0) {
    JsonArray logsArray = doc.createNestedArray("logs");

    // Add most recent logs first (up to 50)
    int count = min(_logEntryCount, 50);
    for (int i = 0; i < count; i++) {
      int index = (_logEntryIndex - 1 - i + MAX_LOG_ENTRIES) % MAX_LOG_ENTRIES;
      if (_logEntries[index].active) {
        JsonObject entry = logsArray.createNestedObject();
        entry["message"] = _logEntries[index].message;
        entry["level"] = _logEntries[index].level;
        entry["timestamp"] = _logEntries[index].timestamp;
      }
    }
  }

  String jsonString;
  serializeJson(doc, jsonString);
  _ws->textAll(jsonString);
}

void WebDashboard::broadcastComponentUpdate(const char* componentId) {
  DashboardComponent* comp = findComponent(componentId);
  if (!comp) {
    return;
  }

  DynamicJsonDocument doc(2048);
  doc["type"] = "component_update";

  JsonObject component = doc.createNestedObject("component");
  component["id"] = comp->id;
  component["type"] = comp->type;
  component["label"] = comp->label;

  // Add component-specific configuration
  switch (comp->type) {
    case ComponentType::SLIDER: {
      JsonObject config = component.createNestedObject("config");
      config["min"] = comp->config.slider.min;
      config["max"] = comp->config.slider.max;
      config["step"] = comp->config.slider.step;
      break;
    }

    case ComponentType::SELECT: {
      JsonObject config = component.createNestedObject("config");
      JsonArray options = config.createNestedArray("options");
      for (int j = 0; j < comp->config.select.optionCount; j++) {
        options.add(comp->config.select.options[j]);
      }
      break;
    }
  }

  // Add component data
  if (comp->data) {
    for (JsonPair kv : comp->data->as<JsonObject>()) {
      component[kv.key().c_str()] = kv.value();
    }
  }

  String jsonString;
  serializeJson(doc, jsonString);
  _ws->textAll(jsonString);
}

WebDashboard::DashboardComponent* WebDashboard::findComponent(const char* id) {
  for (int i = 0; i < _componentCount; i++) {
    if (_components[i].active && strcmp(_components[i].id, id) == 0) {
      return &_components[i];
    }
  }
  return NULL;
}

void WebDashboard::cleanupOldLogs() {
  uint32_t now = millis();

  // Check for rollover
  bool rollover = false;
  for (int i = 0; i < _logEntryCount; i++) {
    if (_logEntries[i].active && _logEntries[i].timestamp > now) {
      // Time has rolled over, adjust timestamps
      rollover = true;
      break;
    }
  }

  if (rollover) {
    // Reset all timestamps relative to now
    for (int i = 0; i < _logEntryCount; i++) {
      if (_logEntries[i].active) {
        _logEntries[i].timestamp = now - (MAX_LOG_RETENTION_TIME / 2);
      }
    }
    return;
  }

  // Remove logs older than retention time
  for (int i = 0; i < _logEntryCount; i++) {
    if (_logEntries[i].active &&
        now - _logEntries[i].timestamp > MAX_LOG_RETENTION_TIME) {
      _logEntries[i].active = false;
    }
  }
}

void WebDashboard::handleNotFound(AsyncWebServerRequest* request) {
  request->send(404, "text/plain", "404: Not Found");
}