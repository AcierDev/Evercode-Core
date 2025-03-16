#include "../include/WebDashboard.h"

#include <LittleFS.h>
#include <Update.h>
#include <WiFi.h>
#include <stdarg.h>

// Add converter for DashCompType
namespace ARDUINOJSON_NAMESPACE {
template <>
struct Converter<DashCompType> {
  static void toJson(DashCompType src, JsonVariant dst) {
    dst.set(static_cast<int>(src));
  }
  static DashCompType fromJson(JsonVariantConst src) {
    return static_cast<DashCompType>(src.as<int>());
  }
};
}  // namespace ARDUINOJSON_NAMESPACE

// Raw HTML for the dashboard webpage (embedded directly in the code)
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>%DASHBOARD_TITLE%</title>
    <style>
        :root {
            --primary-color: #2c3e50;
            --secondary-color: #3498db;
            --success-color: #2ecc71;
            --warning-color: #f39c12;
            --danger-color: #e74c3c;
            --light-color: #ecf0f1;
            --dark-color: #34495e;
            --text-color: #333;
            --text-light: #ecf0f1;
        }

        @media (prefers-color-scheme: dark) {
            :root {
                --primary-color: #1a2530;
                --secondary-color: #2980b9;
                --success-color: #27ae60;
                --warning-color: #d35400;
                --danger-color: #c0392b;
                --light-color: #2c3e50;
                --dark-color: #1a2530;
                --text-color: #ecf0f1;
                --text-light: #ecf0f1;
            }
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: Arial, sans-serif;
            background-color: var(--light-color);
            color: var(--text-color);
            line-height: 1.6;
        }

        header {
            background-color: var(--primary-color);
            color: var(--text-light);
            padding: 1rem;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        h1 {
            font-size: 1.5rem;
            margin: 0;
        }

        .status-indicator {
            display: flex;
            align-items: center;
            font-size: 0.9rem;
        }

        .status-dot {
            height: 10px;
            width: 10px;
            border-radius: 50%;
            display: inline-block;
            margin-right: 5px;
        }

        .connected {
            background-color: var(--success-color);
        }

        .disconnected {
            background-color: var(--danger-color);
        }

        .container {
            padding: 1rem;
        }

        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
            gap: 1rem;
        }

        .component {
            background-color: white;
            border-radius: 5px;
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
            padding: 1rem;
            margin-bottom: 1rem;
        }

        @media (prefers-color-scheme: dark) {
            .component {
                background-color: var(--dark-color);
                box-shadow: 0 2px 5px rgba(0, 0, 0, 0.3);
            }
        }

        .component-header {
            font-weight: bold;
            margin-bottom: 0.5rem;
        }

        button {
            background-color: var(--secondary-color);
            color: white;
            border: none;
            padding: 0.5rem 1rem;
            border-radius: 3px;
            cursor: pointer;
            transition: background-color 0.3s;
        }

        button:hover {
            background-color: #2980b9;
        }

        input[type="range"] {
            width: 100%;
        }

        .switch {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
        }

        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
            border-radius: 34px;
        }

        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
            border-radius: 50%;
        }

        input:checked + .slider {
            background-color: var(--secondary-color);
        }

        input:checked + .slider:before {
            transform: translateX(26px);
        }

        .gauge-container {
            text-align: center;
        }

        .gauge {
            width: 100%;
            max-width: 200px;
            height: 100px;
            margin: 0 auto;
            position: relative;
        }

        .gauge-value {
            font-size: 2rem;
            font-weight: bold;
        }

        .gauge-units {
            font-size: 1rem;
        }

        .state-machine {
            padding: 1rem;
        }

        .state-box {
            display: inline-block;
            padding: 0.5rem 1rem;
            margin: 0.5rem;
            border: 2px solid var(--light-color);
            border-radius: 5px;
        }

        .state-active {
            background-color: var(--secondary-color);
            color: white;
            border-color: var(--secondary-color);
        }

        .chart-container {
            width: 100%;
            height: 200px;
            position: relative;
        }

        .log-container {
            height: 200px;
            overflow-y: auto;
            font-family: monospace;
            background-color: #f5f5f5;
            padding: 0.5rem;
            border-radius: 3px;
        }

        @media (prefers-color-scheme: dark) {
            .log-container {
                background-color: #1a1a1a;
            }
        }

        .log-entry {
            margin-bottom: 0.25rem;
            padding: 0.25rem;
            border-radius: 3px;
        }

        .log-info {
            color: var(--secondary-color);
        }

        .log-warning {
            color: var(--warning-color);
        }

        .log-error {
            color: var(--danger-color);
        }

        .log-debug {
            color: #888;
        }

        .alert-container {
            margin-bottom: 1rem;
        }

        .alert {
            padding: 0.75rem;
            margin-bottom: 0.5rem;
            border-radius: 3px;
        }

        .alert-info {
            background-color: rgba(52, 152, 219, 0.2);
            border-left: 4px solid var(--secondary-color);
        }

        .alert-warning {
            background-color: rgba(243, 156, 18, 0.2);
            border-left: 4px solid var(--warning-color);
        }

        .alert-error {
            background-color: rgba(231, 76, 60, 0.2);
            border-left: 4px solid var(--danger-color);
        }

        @media (max-width: 600px) {
            .dashboard-grid {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <header>
        <h1>%DASHBOARD_TITLE%</h1>
        <div class="status-indicator">
            <span class="status-dot disconnected" id="status-dot"></span>
            <span id="connection-status">Disconnected</span>
        </div>
    </header>
    <div class="container">
        <div class="dashboard-grid" id="dashboard-grid">
            <!-- Components will be dynamically inserted here -->
        </div>
    </div>

    <script>
        const webSocket = new WebSocket('ws://' + window.location.hostname + '/ws');
        const dashboardGrid = document.getElementById('dashboard-grid');
        const statusDot = document.getElementById('status-dot');
        const connectionStatus = document.getElementById('connection-status');
        let components = {};

        // Connection handling
        webSocket.onopen = function(event) {
            statusDot.classList.remove('disconnected');
            statusDot.classList.add('connected');
            connectionStatus.textContent = 'Connected';
            // Request full dashboard data
            webSocket.send(JSON.stringify({ type: 'request_full_update' }));
        };

        webSocket.onclose = function(event) {
            statusDot.classList.remove('connected');
            statusDot.classList.add('disconnected');
            connectionStatus.textContent = 'Disconnected';
            // Try to reconnect after 2 seconds
            setTimeout(function() {
                window.location.reload();
            }, 2000);
        };

        webSocket.onerror = function(error) {
            console.error('WebSocket error:', error);
        };

        // Message handling
        webSocket.onmessage = function(event) {
            const data = JSON.parse(event.data);
            
            if (data.type === 'full_update') {
                handleFullUpdate(data);
            } else if (data.type === 'component_update') {
                handleComponentUpdate(data);
            } else if (data.type === 'log') {
                handleLogUpdate(data);
            } else if (data.type === 'alert') {
                handleAlertUpdate(data);
            }
        };

        // Handle full dashboard update
        function handleFullUpdate(data) {
            dashboardGrid.innerHTML = '';
            components = {};
            
            data.components.forEach(component => {
                createComponent(component);
            });
        }

        // Handle single component update
        function handleComponentUpdate(data) {
            const component = data.component;
            const componentElement = document.getElementById(`component-${component.id}`);
            
            if (componentElement) {
                updateComponentValue(component);
            } else {
                createComponent(component);
            }
        }

        // Create a new component
        function createComponent(component) {
            components[component.id] = component;
            
            const componentElement = document.createElement('div');
            componentElement.className = 'component';
            componentElement.id = `component-${component.id}`;
            
            let componentContent = '';
            
            switch (component.type) {
                case 1: // Button
                    componentContent = createButton(component);
                    break;
                case 2: // Switch
                    componentContent = createSwitch(component);
                    break;
                case 3: // Slider
                    componentContent = createSlider(component);
                    break;
                case 4: // Gauge
                    componentContent = createGauge(component);
                    break;
                case 5: // Chart
                    componentContent = createChart(component);
                    break;
                case 6: // Text
                    componentContent = createText(component);
                    break;
                case 7: // Log
                    componentContent = createLog(component);
                    break;
                case 8: // Alert
                    componentContent = createAlert(component);
                    break;
                case 9: // State Machine
                    componentContent = createStateMachine(component);
                    break;
                case 10: // Status
                    componentContent = createStatus(component);
                    break;
            }
            
            componentElement.innerHTML = componentContent;
            dashboardGrid.appendChild(componentElement);
            
            // Initialize any special components after adding to DOM
            if (component.type === 5) { // Chart
                initializeChart(component);
            }
        }

        // Update component value
        function updateComponentValue(component) {
            components[component.id] = component;
            
            switch (component.type) {
                case 1: // Button - no value update needed
                    break;
                case 2: // Switch
                    updateSwitch(component);
                    break;
                case 3: // Slider
                    updateSlider(component);
                    break;
                case 4: // Gauge
                    updateGauge(component);
                    break;
                case 5: // Chart
                    updateChart(component);
                    break;
                case 6: // Text
                    updateText(component);
                    break;
                case 7: // Log
                    updateLog(component);
                    break;
                case 8: // Alert
                    updateAlert(component);
                    break;
                case 9: // State Machine
                    updateStateMachine(component);
                    break;
                case 10: // Status
                    updateStatus(component);
                    break;
            }
        }

        // Component creation functions
        function createButton(component) {
            return `
                <div class="component-header">${component.label}</div>
                <button id="button-${component.id}" onclick="sendButtonPress('${component.id}')">
                    ${component.label}
                </button>
            `;
        }

        function createSwitch(component) {
            const checked = component.value ? 'checked' : '';
            return `
                <div class="component-header">${component.label}</div>
                <label class="switch">
                    <input type="checkbox" id="switch-${component.id}" ${checked} 
                        onchange="sendSwitchToggle('${component.id}', this.checked)">
                    <span class="slider"></span>
                </label>
            `;
        }

        function createSlider(component) {
            return `
                <div class="component-header">${component.label}</div>
                <input type="range" id="slider-${component.id}" 
                    min="${component.config.min}" max="${component.config.max}" 
                    value="${component.value}" 
                    oninput="document.getElementById('slider-value-${component.id}').textContent = this.value"
                    onchange="sendSliderChange('${component.id}', this.value)">
                <div>Value: <span id="slider-value-${component.id}">${component.value}</span></div>
            `;
        }

        function createGauge(component) {
            return `
                <div class="component-header">${component.label}</div>
                <div class="gauge-container">
                    <div class="gauge">
                        <div class="gauge-value" id="gauge-value-${component.id}">${component.value}</div>
                        <div class="gauge-units">${component.config.units}</div>
                    </div>
                </div>
            `;
        }

        function createChart(component) {
            return `
                <div class="component-header">${component.label}</div>
                <div class="chart-container">
                    <canvas id="chart-${component.id}"></canvas>
                </div>
            `;
        }

        function createText(component) {
            return `
                <div class="component-header">${component.label}</div>
                <div id="text-${component.id}">${component.value}</div>
            `;
        }

        function createLog(component) {
            return `
                <div class="component-header">${component.label}</div>
                <div class="log-container" id="log-${component.id}"></div>
            `;
        }

        function createAlert(component) {
            return `
                <div class="component-header">${component.label}</div>
                <div class="alert-container" id="alert-${component.id}"></div>
            `;
        }

        function createStateMachine(component) {
            let states = '';
            component.config.states.forEach(state => {
                const activeClass = state === component.value ? 'state-active' : '';
                states += `<div class="state-box ${activeClass}" 
                    onclick="sendStateChange('${component.id}', '${state}')">${state}</div>`;
            });
            
            return `
                <div class="component-header">${component.label}</div>
                <div class="state-machine" id="state-${component.id}">
                    ${states}
                </div>
            `;
        }

        function createStatus(component) {
            return `
                <div class="component-header">${component.label}</div>
                <div id="status-${component.id}">${component.value}</div>
            `;
        }

        // Component update functions
        function updateSwitch(component) {
            const switchElement = document.getElementById(`switch-${component.id}`);
            if (switchElement) {
                switchElement.checked = component.value;
            }
        }

        function updateSlider(component) {
            const sliderElement = document.getElementById(`slider-${component.id}`);
            const valueElement = document.getElementById(`slider-value-${component.id}`);
            if (sliderElement && valueElement) {
                sliderElement.value = component.value;
                valueElement.textContent = component.value;
            }
        }

        function updateGauge(component) {
            const gaugeElement = document.getElementById(`gauge-value-${component.id}`);
            if (gaugeElement) {
                gaugeElement.textContent = component.value;
            }
        }

        function updateChart(component) {
            // Chart update logic would go here
            // This is simplified - in a real implementation you would use a charting library
            console.log('Chart update for', component.id, component.data);
        }

        function updateText(component) {
            const textElement = document.getElementById(`text-${component.id}`);
            if (textElement) {
                textElement.textContent = component.value;
            }
        }

        function updateLog(component) {
            const logContainer = document.getElementById(`log-${component.id}`);
            if (logContainer && component.data && component.data.entries) {
                logContainer.innerHTML = '';
                component.data.entries.forEach(entry => {
                    const logClass = entry.level === 0 ? 'log-info' : 
                                    entry.level === 1 ? 'log-warning' : 
                                    entry.level === 2 ? 'log-error' : 'log-debug';
                    
                    const entryElement = document.createElement('div');
                    entryElement.className = `log-entry ${logClass}`;
                    entryElement.textContent = `[${new Date(entry.timestamp).toLocaleTimeString()}] ${entry.message}`;
                    logContainer.appendChild(entryElement);
                });
                logContainer.scrollTop = logContainer.scrollHeight;
            }
        }

        function updateAlert(component) {
            const alertContainer = document.getElementById(`alert-${component.id}`);
            if (alertContainer && component.data && component.data.alerts) {
                alertContainer.innerHTML = '';
                component.data.alerts.forEach(alert => {
                    const alertClass = alert.level === 0 ? 'alert-info' : 
                                      alert.level === 1 ? 'alert-warning' : 'alert-error';
                    
                    const alertElement = document.createElement('div');
                    alertElement.className = `alert ${alertClass}`;
                    alertElement.textContent = alert.message;
                    alertContainer.appendChild(alertElement);
                });
            }
        }

        function updateStateMachine(component) {
            const stateContainer = document.getElementById(`state-${component.id}`);
            if (stateContainer) {
                const stateBoxes = stateContainer.querySelectorAll('.state-box');
                stateBoxes.forEach(box => {
                    if (box.textContent === component.value) {
                        box.classList.add('state-active');
                    } else {
                        box.classList.remove('state-active');
                    }
                });
            }
        }

        function updateStatus(component) {
            const statusElement = document.getElementById(`status-${component.id}`);
            if (statusElement) {
                statusElement.textContent = component.value;
            }
        }

        // Handle log updates
        function handleLogUpdate(data) {
            const logComponents = Object.values(components).filter(c => c.type === 7);
            logComponents.forEach(component => {
                if (!component.data) component.data = { entries: [] };
                if (!component.data.entries) component.data.entries = [];
                
                component.data.entries.push(data.entry);
                if (component.data.entries.length > component.config.maxEntries) {
                    component.data.entries.shift();
                }
                
                updateLog(component);
            });
        }

        // Handle alert updates
        function handleAlertUpdate(data) {
            const alertComponents = Object.values(components).filter(c => c.type === 8);
            alertComponents.forEach(component => {
                if (!component.data) component.data = { alerts: [] };
                if (!component.data.alerts) component.data.alerts = [];
                
                component.data.alerts.push(data.alert);
                updateAlert(component);
            });
        }

        // UI interaction functions
        function sendButtonPress(id) {
            webSocket.send(JSON.stringify({
                type: 'button_press',
                id: id
            }));
        }

        function sendSwitchToggle(id, state) {
            webSocket.send(JSON.stringify({
                type: 'switch_toggle',
                id: id,
                value: state
            }));
        }

        function sendSliderChange(id, value) {
            webSocket.send(JSON.stringify({
                type: 'slider_change',
                id: id,
                value: parseInt(value)
            }));
        }

        function sendStateChange(id, state) {
            webSocket.send(JSON.stringify({
                type: 'state_change',
                id: id,
                value: state
            }));
        }

        // Chart initialization
        function initializeChart(component) {
            // This is a placeholder - in a real implementation you would use a charting library
            console.log('Initializing chart', component.id);
        }

        // Helper function to escape HTML
        function escapeHtml(text) {
            return text
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/"/g, "&quot;")
                .replace(/'/g, "&#039;");
        }
    </script>
</body>
</html>
)rawliteral";

// Constructor
WebDashboard::WebDashboard() {
  _isInitialized = false;
  _debugLoggingEnabled = false;
  _lastUpdate = 0;
  _authEnabled = false;
  _corsEnabled = false;
  _componentCount = 0;
  _clientCount = 0;
  _logEntryCount = 0;
  _logEntryIndex = 0;
  _clientConnectCallback = NULL;
  memset(_dashboardTitle, 0, sizeof(_dashboardTitle));
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
    if (_authEnabled) {
      this->handleAuthentication(request);
      return;
    }

    // Replace placeholders in the HTML template
    String html = FPSTR(DASHBOARD_HTML);
    html.replace("%DASHBOARD_TITLE%", _dashboardTitle);

    AsyncWebServerResponse* response =
        request->beginResponse(200, "text/html", html);

    // Add CORS headers if enabled
    if (_corsEnabled) {
      this->addCORS(response);
    }

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
bool WebDashboard::registerButton(const char* id, const char* label,
                                  ButtonPressCallback callback) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = DashCompType::BUTTON;
  comp->active = true;
  comp->callback = (void*)callback;
  comp->data = NULL;

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::registerSwitch(const char* id, const char* label,
                                  bool initialState,
                                  SwitchToggleCallback callback) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = DashCompType::SWITCH;
  comp->active = true;
  comp->callback = (void*)callback;

  comp->data = new DynamicJsonDocument(64);
  if (comp->data) {
    (*comp->data)["value"] = initialState;
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::registerSlider(const char* id, const char* label, int min,
                                  int max, int initialValue,
                                  SliderChangeCallback callback) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = DashCompType::SLIDER;
  comp->active = true;
  comp->callback = (void*)callback;

  comp->config.gauge.min = min;
  comp->config.gauge.max = max;

  comp->data = new DynamicJsonDocument(64);
  if (comp->data) {
    (*comp->data)["value"] = initialValue;
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::registerGauge(const char* id, const char* label, int min,
                                 int max, int initialValue, const char* units) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = DashCompType::GAUGE;
  comp->active = true;
  comp->callback = NULL;

  comp->config.gauge.min = min;
  comp->config.gauge.max = max;
  strncpy(comp->config.gauge.units, units,
          sizeof(comp->config.gauge.units) - 1);

  comp->data = new DynamicJsonDocument(64);
  if (comp->data) {
    (*comp->data)["value"] = initialValue;
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::registerText(const char* id, const char* label,
                                const char* initialValue) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = DashCompType::TEXT;
  comp->active = true;
  comp->callback = NULL;

  comp->data = new DynamicJsonDocument(256);
  if (comp->data) {
    (*comp->data)["value"] = initialValue;
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::registerStatus(const char* id, const char* label,
                                  const char* initialValue) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, label, sizeof(comp->label) - 1);
  comp->type = DashCompType::STATUS;
  comp->active = true;
  comp->callback = NULL;

  comp->data = new DynamicJsonDocument(256);
  if (comp->data) {
    (*comp->data)["value"] = initialValue;
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::registerChart(const char* id, const char* title,
                                 const char* xLabel, const char* yLabel,
                                 int maxDataPoints) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, title, sizeof(comp->label) - 1);
  comp->type = DashCompType::CHART;
  comp->active = true;
  comp->callback = NULL;

  strncpy(comp->config.chart.xLabel, xLabel,
          sizeof(comp->config.chart.xLabel) - 1);
  strncpy(comp->config.chart.yLabel, yLabel,
          sizeof(comp->config.chart.yLabel) - 1);
  comp->config.chart.maxPoints = maxDataPoints;

  comp->data = new DynamicJsonDocument(2048);  // Larger size for chart data
  if (comp->data) {
    JsonArray dataPoints = (*comp->data).createNestedArray("dataPoints");
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::registerLogDisplay(const char* id, const char* title,
                                      int maxLogEntries) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, title, sizeof(comp->label) - 1);
  comp->type = DashCompType::LOG;
  comp->active = true;
  comp->callback = NULL;

  comp->config.logDisplay.maxEntries = maxLogEntries;

  comp->data = new DynamicJsonDocument(2048);  // Larger size for log entries
  if (comp->data) {
    JsonArray entries = (*comp->data).createNestedArray("entries");
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::registerAlertDisplay(const char* id, const char* title) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, title, sizeof(comp->label) - 1);
  comp->type = DashCompType::ALERT;
  comp->active = true;
  comp->callback = NULL;

  comp->data = new DynamicJsonDocument(1024);  // Size for alert entries
  if (comp->data) {
    JsonArray alerts = (*comp->data).createNestedArray("alerts");
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::registerStateMachine(const char* id, const char* title,
                                        const char** states, int stateCount,
                                        const char* initialState,
                                        StateChangeCallback callback) {
  if (_componentCount >= MAX_DASHBOARD_COMPONENTS || stateCount <= 0) {
    return false;
  }

  DashboardComponent* comp = &_components[_componentCount++];
  strncpy(comp->id, id, MAX_COMPONENT_ID_LENGTH - 1);
  strncpy(comp->label, title, sizeof(comp->label) - 1);
  comp->type = DashCompType::STATE;
  comp->active = true;
  comp->callback = (void*)callback;

  // Allocate and copy states
  comp->config.stateMachine.states = new char*[stateCount];
  comp->config.stateMachine.stateCount = stateCount;
  for (int i = 0; i < stateCount; i++) {
    comp->config.stateMachine.states[i] = new char[MAX_STATE_NAME_LENGTH];
    strncpy(comp->config.stateMachine.states[i], states[i],
            MAX_STATE_NAME_LENGTH - 1);
  }

  strncpy(comp->config.stateMachine.currentState, initialState,
          MAX_STATE_NAME_LENGTH - 1);

  comp->data = new DynamicJsonDocument(256);
  if (comp->data) {
    (*comp->data)["value"] = initialState;
    JsonArray statesArray = (*comp->data).createNestedArray("states");
    for (int i = 0; i < stateCount; i++) {
      statesArray.add(states[i]);
    }
  }

  broadcastComponentUpdate(id);

  return true;
}

bool WebDashboard::unregisterComponent(const char* id) {
  DashboardComponent* comp = findComponent(id);
  if (!comp) {
    return false;
  }

  comp->active = false;

  // Free allocated memory
  if (comp->data) {
    delete comp->data;
    comp->data = NULL;
  }

  // Free states array for state machine components
  if (comp->type == DashCompType::STATE && comp->config.stateMachine.states) {
    for (int i = 0; i < comp->config.stateMachine.stateCount; i++) {
      delete[] comp->config.stateMachine.states[i];
    }
    delete[] comp->config.stateMachine.states;
    comp->config.stateMachine.states = NULL;
  }

  // Broadcast the removal (by sending an empty update)
  DynamicJsonDocument doc(64);
  doc["type"] = "remove_component";
  doc["id"] = id;

  String jsonString;
  serializeJson(doc, jsonString);
  _ws->textAll(jsonString);

  return true;
}

// Component update methods
bool WebDashboard::updateComponent(const char* id, const char* value) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data) {
    return false;
  }

  (*comp->data)["value"] = value;
  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::updateComponent(const char* id, int value) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data) {
    return false;
  }

  (*comp->data)["value"] = value;
  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::updateComponent(const char* id, float value, int precision) {
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

bool WebDashboard::updateComponent(const char* id, bool value) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data) {
    return false;
  }

  (*comp->data)["value"] = value;
  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::addChartDataPoint(const char* id, float x, float y) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data || comp->type != DashCompType::CHART) {
    return false;
  }

  JsonArray dataPoints;
  if ((*comp->data).containsKey("dataPoints")) {
    dataPoints = (*comp->data)["dataPoints"];
  } else {
    dataPoints = (*comp->data).createNestedArray("dataPoints");
  }

  // Check if we need to remove old data points
  while (dataPoints.size() >= comp->config.chart.maxPoints) {
    // Remove the oldest data point
    for (size_t i = 0; i < dataPoints.size() - 1; i++) {
      dataPoints[i] = dataPoints[i + 1];
    }
    dataPoints.remove(dataPoints.size() - 1);
  }

  // Add new data point
  JsonObject point = dataPoints.createNestedObject();
  point["x"] = x;
  point["y"] = y;

  broadcastComponentUpdate(id);
  return true;
}

bool WebDashboard::addChartDataPoint(const char* id, float y) {
  // Use current timestamp for x-axis
  return addChartDataPoint(id, millis() / 1000.0, y);
}

bool WebDashboard::clearChartData(const char* id) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data || comp->type != DashCompType::CHART) {
    return false;
  }

  // Clear and recreate the data points array
  (*comp->data).remove("dataPoints");
  (*comp->data).createNestedArray("dataPoints");

  broadcastComponentUpdate(id);
  return true;
}

// State machine methods
bool WebDashboard::updateState(const char* id, const char* state) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data || comp->type != DashCompType::STATE) {
    return false;
  }

  // Make sure the new state is valid
  bool validState = false;
  for (int i = 0; i < comp->config.stateMachine.stateCount; i++) {
    if (strcmp(comp->config.stateMachine.states[i], state) == 0) {
      validState = true;
      break;
    }
  }

  if (!validState) {
    return false;
  }

  // Store the old state for the callback
  char oldState[MAX_STATE_NAME_LENGTH];
  strncpy(oldState, comp->config.stateMachine.currentState,
          MAX_STATE_NAME_LENGTH - 1);

  // Update the state
  strncpy(comp->config.stateMachine.currentState, state,
          MAX_STATE_NAME_LENGTH - 1);
  (*comp->data)["value"] = state;

  // Notify clients
  broadcastComponentUpdate(id);

  return true;
}

String WebDashboard::getCurrentState(const char* id) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || comp->type != DashCompType::STATE) {
    return "";
  }

  return String(comp->config.stateMachine.currentState);
}

bool WebDashboard::onStateChange(const char* id, StateChangeCallback callback) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || comp->type != DashCompType::STATE) {
    return false;
  }

  comp->callback = (void*)callback;
  return true;
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

bool WebDashboard::alert(const char* message, uint8_t level) {
  if (!_isInitialized) {
    return false;
  }

  // Send to all alert display components
  DynamicJsonDocument doc(512);
  doc["type"] = "alert";
  JsonObject alertObj = doc.createNestedObject("alert");
  alertObj["message"] = message;
  alertObj["level"] = level;
  alertObj["timestamp"] = millis();

  String jsonString;
  serializeJson(doc, jsonString);
  _ws->textAll(jsonString);

  // Also log the alert
  const char* levelStr = level == 0 ? "INFO" : level == 1 ? "WARNING" : "ERROR";
  char logMessage[MAX_LOG_LENGTH];
  snprintf(logMessage, MAX_LOG_LENGTH - 1, "ALERT [%s]: %s", levelStr, message);
  log(logMessage, level);

  return true;
}

bool WebDashboard::alertf(uint8_t level, const char* format, ...) {
  char buffer[MAX_ALERT_LENGTH];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, MAX_ALERT_LENGTH - 1, format, args);
  va_end(args);

  return alert(buffer, level);
}

bool WebDashboard::clearAlerts() {
  if (!_isInitialized) {
    return false;
  }

  // Send clear command to all alert components
  for (int i = 0; i < _componentCount; i++) {
    if (_components[i].active && _components[i].type == DashCompType::ALERT &&
        _components[i].data) {
      (*_components[i].data).remove("alerts");
      (*_components[i].data).createNestedArray("alerts");

      broadcastComponentUpdate(_components[i].id);
    }
  }

  return true;
}

// Client management
int WebDashboard::getConnectedClientCount() {
  int count = 0;
  for (int i = 0; i < _clientCount; i++) {
    if (_clients[i].active) {
      count++;
    }
  }
  return count;
}

bool WebDashboard::onClientConnect(WebClientConnectCallback callback) {
  _clientConnectCallback = callback;
  return true;
}

bool WebDashboard::setAuthentication(const char* username,
                                     const char* password) {
  strncpy(_authUsername, username, sizeof(_authUsername) - 1);
  strncpy(_authPassword, password, sizeof(_authPassword) - 1);
  _authEnabled = true;
  return true;
}

bool WebDashboard::isAuthenticationEnabled() { return _authEnabled; }

bool WebDashboard::enableCORS(bool enable) {
  _corsEnabled = enable;
  return true;
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

    if (comp && comp->type == DashCompType::BUTTON && comp->callback) {
      ButtonPressCallback callback = (ButtonPressCallback)comp->callback;
      callback(id);
    }

  } else if (strcmp(type, "switch_toggle") == 0) {
    // Switch toggle event
    const char* id = doc["id"];
    bool value = doc["value"];

    DashboardComponent* comp = findComponent(id);

    if (comp && comp->type == DashCompType::SWITCH && comp->data) {
      // Update internal state
      (*comp->data)["value"] = value;

      // Call callback if set
      if (comp->callback) {
        SwitchToggleCallback callback = (SwitchToggleCallback)comp->callback;
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

    if (comp && comp->type == DashCompType::SLIDER && comp->data) {
      // Update internal state
      (*comp->data)["value"] = value;

      // Call callback if set
      if (comp->callback) {
        SliderChangeCallback callback = (SliderChangeCallback)comp->callback;
        callback(id, value);
      }

      // Broadcast the update to all clients
      broadcastComponentUpdate(id);
    }

  } else if (strcmp(type, "state_change") == 0) {
    // State change event
    const char* id = doc["id"];
    const char* value = doc["value"];

    DashboardComponent* comp = findComponent(id);

    if (comp && comp->type == DashCompType::STATE && comp->data) {
      // Save old state
      char oldState[MAX_STATE_NAME_LENGTH];
      strncpy(oldState, comp->config.stateMachine.currentState,
              MAX_STATE_NAME_LENGTH - 1);

      // Update internal state
      strncpy(comp->config.stateMachine.currentState, value,
              MAX_STATE_NAME_LENGTH - 1);
      (*comp->data)["value"] = value;

      // Call callback if set
      if (comp->callback) {
        StateChangeCallback callback = (StateChangeCallback)comp->callback;
        callback(id, oldState, value);
      }

      // Broadcast the update to all clients
      broadcastComponentUpdate(id);
    }
  }
}

void WebDashboard::broadcastDashboardUpdate(bool fullUpdate) {
  DynamicJsonDocument doc(8192);  // Large document for full dashboard
  doc["type"] = "full_update";

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
        case DashCompType::GAUGE:
        case DashCompType::SLIDER: {
          JsonObject config = component.createNestedObject("config");
          config["min"] = _components[i].config.gauge.min;
          config["max"] = _components[i].config.gauge.max;
          if (_components[i].type == DashCompType::GAUGE) {
            config["units"] = _components[i].config.gauge.units;
          }
          break;
        }

        case DashCompType::CHART: {
          JsonObject chartConfig = component.createNestedObject("config");
          chartConfig["xLabel"] = _components[i].config.chart.xLabel;
          chartConfig["yLabel"] = _components[i].config.chart.yLabel;
          chartConfig["maxPoints"] = _components[i].config.chart.maxPoints;
          break;
        }

        case DashCompType::STATE: {
          JsonObject stateConfig = component.createNestedObject("config");
          JsonArray states = stateConfig.createNestedArray("states");
          for (int j = 0; j < _components[i].config.stateMachine.stateCount;
               j++) {
            states.add(_components[i].config.stateMachine.states[j]);
          }
          break;
        }

        case DashCompType::LOG: {
          JsonObject logConfig = component.createNestedObject("config");
          logConfig["maxEntries"] = _components[i].config.logDisplay.maxEntries;
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
    case DashCompType::GAUGE:
    case DashCompType::SLIDER: {
      JsonObject config = component.createNestedObject("config");
      config["min"] = comp->config.gauge.min;
      config["max"] = comp->config.gauge.max;
      if (comp->type == DashCompType::GAUGE) {
        config["units"] = comp->config.gauge.units;
      }
      break;
    }

    case DashCompType::CHART: {
      JsonObject chartConfig = component.createNestedObject("config");
      chartConfig["xLabel"] = comp->config.chart.xLabel;
      chartConfig["yLabel"] = comp->config.chart.yLabel;
      chartConfig["maxPoints"] = comp->config.chart.maxPoints;
      break;
    }

    case DashCompType::STATE: {
      JsonObject stateConfig = component.createNestedObject("config");
      JsonArray states = stateConfig.createNestedArray("states");
      for (int j = 0; j < comp->config.stateMachine.stateCount; j++) {
        states.add(comp->config.stateMachine.states[j]);
      }
      break;
    }

    case DashCompType::LOG: {
      JsonObject logConfig = component.createNestedObject("config");
      logConfig["maxEntries"] = comp->config.logDisplay.maxEntries;
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
        _logEntries[i].timestamp = now - (LOG_RETENTION_TIME / 2);
      }
    }
    return;
  }

  // Remove logs older than retention time
  for (int i = 0; i < _logEntryCount; i++) {
    if (_logEntries[i].active &&
        now - _logEntries[i].timestamp > LOG_RETENTION_TIME) {
      _logEntries[i].active = false;
    }
  }
}

void WebDashboard::handleNotFound(AsyncWebServerRequest* request) {
  if (_authEnabled) {
    handleAuthentication(request);
    return;
  }

  request->send(404, "text/plain", "404: Not Found");
}

void WebDashboard::handleAuthentication(AsyncWebServerRequest* request) {
  if (!request->authenticate(_authUsername, _authPassword)) {
    return request->requestAuthentication();
  }

  // If we get here, authentication was successful
  if (request->url() == "/") {
    // Replace placeholders in the HTML template
    String html = FPSTR(DASHBOARD_HTML);
    html.replace("%DASHBOARD_TITLE%", _dashboardTitle);

    AsyncWebServerResponse* response =
        request->beginResponse(200, "text/html", html);

    // Add CORS headers if enabled
    if (_corsEnabled) {
      addCORS(response);
    }

    request->send(response);
  } else {
    request->send(404, "text/plain", "404: Not Found");
  }
}

void WebDashboard::addCORS(AsyncWebServerResponse* response) {
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods",
                      "GET, POST, PUT, DELETE, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
}

bool WebDashboard::isComponentValue(const char* id, bool expectedValue) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data || !(*comp->data).containsKey("value")) {
    return false;
  }

  return (*comp->data)["value"].as<bool>() == expectedValue;
}

bool WebDashboard::isComponentValue(const char* id, const char* expectedValue) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data || !(*comp->data).containsKey("value")) {
    return false;
  }

  return strcmp((*comp->data)["value"].as<const char*>(), expectedValue) == 0;
}

bool WebDashboard::isComponentValue(const char* id, int expectedValue) {
  DashboardComponent* comp = findComponent(id);
  if (!comp || !comp->data || !(*comp->data).containsKey("value")) {
    return false;
  }

  return (*comp->data)["value"].as<int>() == expectedValue;
}