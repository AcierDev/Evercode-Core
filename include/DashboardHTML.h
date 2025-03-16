/**
 * DashboardHTML.h - HTML template for WebDashboard
 *
 * A minimal, efficient HTML template for the dashboard with a focus on:
 * - Dense, information-rich UI
 * - Low overhead and memory usage
 * - Fast loading and responsive design
 */

#ifndef DashboardHTML_h
#define DashboardHTML_h

// Raw HTML for the dashboard webpage
static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>%DASHBOARD_TITLE%</title>
    <style>
        :root {
            --primary: #1a1a1a;
            --secondary: #2c3e50;
            --accent: #3498db;
            --success: #2ecc71;
            --warning: #f39c12;
            --danger: #e74c3c;
            --light: #f8f9fa;
            --dark: #343a40;
            --text: #f8f9fa;
            --border: #555;
            --radius: 4px;
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif;
        }

        body {
            background-color: var(--primary);
            color: var(--text);
            font-size: 14px;
            line-height: 1.4;
        }

        header {
            background-color: var(--secondary);
            padding: 8px 16px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            border-bottom: 1px solid var(--border);
            position: sticky;
            top: 0;
            z-index: 100;
        }

        .header-left {
            display: flex;
            align-items: center;
        }

        .header-title {
            font-size: 16px;
            font-weight: 600;
            margin-right: 16px;
        }

        .machine-state {
            background-color: var(--accent);
            color: white;
            padding: 4px 8px;
            border-radius: var(--radius);
            font-size: 12px;
            font-weight: 500;
        }

        .connection-status {
            display: flex;
            align-items: center;
            font-size: 12px;
        }

        .status-dot {
            height: 8px;
            width: 8px;
            border-radius: 50%;
            margin-right: 6px;
        }

        .connected {
            background-color: var(--success);
            box-shadow: 0 0 5px var(--success);
        }

        .disconnected {
            background-color: var(--danger);
            box-shadow: 0 0 5px var(--danger);
        }

        nav {
            background-color: var(--secondary);
            display: flex;
            border-bottom: 1px solid var(--border);
        }

        .nav-tab {
            padding: 8px 16px;
            cursor: pointer;
            border-right: 1px solid var(--border);
            font-size: 14px;
            transition: background-color 0.2s;
        }

        .nav-tab:hover {
            background-color: rgba(255, 255, 255, 0.1);
        }

        .nav-tab.active {
            background-color: var(--accent);
            color: white;
        }

        .content {
            padding: 16px;
        }

        .section {
            display: none;
        }

        .section.active {
            display: block;
        }

        .section-title {
            font-size: 16px;
            font-weight: 600;
            margin-bottom: 12px;
            padding-bottom: 6px;
            border-bottom: 1px solid var(--border);
        }

        /* Monitoring section */
        .monitoring-grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 16px;
        }

        .log-container {
            background-color: var(--dark);
            border: 1px solid var(--border);
            border-radius: var(--radius);
            height: 300px;
            overflow-y: auto;
            font-family: monospace;
            font-size: 12px;
            padding: 8px;
        }

        .log-entry {
            margin-bottom: 4px;
            padding: 2px 4px;
            border-radius: 2px;
        }

        .log-info {
            color: var(--light);
        }

        .log-warning {
            color: var(--warning);
        }

        .log-error {
            color: var(--danger);
        }

        .log-debug {
            color: #888;
        }

        .pin-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
            gap: 8px;
        }

        .pin-monitor {
            background-color: var(--dark);
            border: 1px solid var(--border);
            border-radius: var(--radius);
            padding: 8px;
            position: relative;
        }

        .pin-label {
            font-size: 12px;
            margin-bottom: 4px;
            font-weight: 500;
        }

        .pin-value {
            font-size: 14px;
            font-weight: 600;
            text-align: center;
            margin-bottom: 4px;
        }

        .pin-bar {
            height: 6px;
            background-color: #555;
            border-radius: 3px;
            overflow: hidden;
        }

        .pin-progress {
            height: 100%;
            background-color: var(--accent);
            transition: width 0.2s;
        }

        .pin-digital {
            position: absolute;
            top: 8px;
            right: 8px;
            height: 8px;
            width: 8px;
            border-radius: 50%;
        }

        .pin-on {
            background-color: var(--success);
            box-shadow: 0 0 5px var(--success);
        }

        .pin-off {
            background-color: #555;
        }

        /* Controls section */
        .controls-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(150px, 1fr));
            gap: 12px;
        }

        .control-button {
            background-color: var(--accent);
            color: white;
            border: none;
            border-radius: var(--radius);
            padding: 8px 12px;
            cursor: pointer;
            font-size: 14px;
            font-weight: 500;
            transition: background-color 0.2s;
            width: 100%;
            text-align: center;
        }

        .control-button:hover {
            background-color: #2980b9;
        }

        .control-button:active {
            transform: translateY(1px);
        }

        /* Settings section */
        .settings-grid {
            display: grid;
            grid-template-columns: 1fr;
            gap: 12px;
        }

        .setting-item {
            background-color: var(--dark);
            border: 1px solid var(--border);
            border-radius: var(--radius);
            padding: 12px;
            display: flex;
            align-items: center;
            justify-content: space-between;
        }

        .setting-label {
            font-size: 14px;
            font-weight: 500;
        }

        /* Toggle */
        .toggle-switch {
            position: relative;
            display: inline-block;
            width: 40px;
            height: 20px;
        }

        .toggle-switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .toggle-slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #555;
            transition: .3s;
            border-radius: 20px;
        }

        .toggle-slider:before {
            position: absolute;
            content: "";
            height: 16px;
            width: 16px;
            left: 2px;
            bottom: 2px;
            background-color: white;
            transition: .3s;
            border-radius: 50%;
        }

        input:checked + .toggle-slider {
            background-color: var(--accent);
        }

        input:checked + .toggle-slider:before {
            transform: translateX(20px);
        }

        /* Slider */
        .slider-container {
            width: 100%;
            max-width: 200px;
        }

        .slider-input {
            width: 100%;
            -webkit-appearance: none;
            height: 6px;
            border-radius: 3px;
            background: #555;
            outline: none;
        }

        .slider-input::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 16px;
            height: 16px;
            border-radius: 50%;
            background: var(--accent);
            cursor: pointer;
        }

        .slider-input::-moz-range-thumb {
            width: 16px;
            height: 16px;
            border-radius: 50%;
            background: var(--accent);
            cursor: pointer;
            border: none;
        }

        .slider-value {
            text-align: center;
            font-size: 12px;
            margin-top: 4px;
        }

        /* Text input */
        .text-input {
            background-color: #333;
            border: 1px solid var(--border);
            border-radius: var(--radius);
            color: var(--text);
            padding: 6px 8px;
            font-size: 14px;
            width: 100%;
            max-width: 200px;
        }

        /* Select */
        .select-input {
            background-color: #333;
            border: 1px solid var(--border);
            border-radius: var(--radius);
            color: var(--text);
            padding: 6px 8px;
            font-size: 14px;
            width: 100%;
            max-width: 200px;
        }

        /* Responsive adjustments */
        @media (max-width: 768px) {
            .monitoring-grid {
                grid-template-columns: 1fr;
            }
            
            .pin-grid {
                grid-template-columns: repeat(auto-fill, minmax(100px, 1fr));
            }
            
            .controls-grid {
                grid-template-columns: repeat(auto-fill, minmax(120px, 1fr));
            }
        }
    </style>
</head>
<body>
    <header>
        <div class="header-left">
            <div class="header-title">%DASHBOARD_TITLE%</div>
            <div class="machine-state" id="machine-state">UNKNOWN</div>
        </div>
        <div class="connection-status">
            <span class="status-dot disconnected" id="status-dot"></span>
            <span id="connection-status">Disconnected</span>
        </div>
    </header>

    <nav>
        <div class="nav-tab active" data-section="monitoring">Monitoring</div>
        <div class="nav-tab" data-section="controls">Controls</div>
        <div class="nav-tab" data-section="settings">Settings</div>
    </nav>

    <div class="content">
        <div id="monitoring" class="section active">
            <h2 class="section-title">Monitoring</h2>
            <div class="monitoring-grid">
                <div>
                    <h3 class="section-title">Logs</h3>
                    <div class="log-container" id="log-container"></div>
                </div>
                <div>
                    <h3 class="section-title">Pin Monitoring</h3>
                    <div class="pin-grid" id="pin-grid"></div>
                </div>
            </div>
        </div>

        <div id="controls" class="section">
            <h2 class="section-title">Controls</h2>
            <div class="controls-grid" id="controls-grid"></div>
        </div>

        <div id="settings" class="section">
            <h2 class="section-title">Settings</h2>
            <div class="settings-grid" id="settings-grid"></div>
        </div>
    </div>

    <script>
        // WebSocket connection
        const webSocket = new WebSocket('ws://' + window.location.hostname + '/ws');
        const statusDot = document.getElementById('status-dot');
        const connectionStatus = document.getElementById('connection-status');
        const machineState = document.getElementById('machine-state');
        
        // Section containers
        const logContainer = document.getElementById('log-container');
        const pinGrid = document.getElementById('pin-grid');
        const controlsGrid = document.getElementById('controls-grid');
        const settingsGrid = document.getElementById('settings-grid');
        
        // Track components
        let components = {};

        // Navigation
        document.querySelectorAll('.nav-tab').forEach(tab => {
            tab.addEventListener('click', function() {
                // Update active tab
                document.querySelectorAll('.nav-tab').forEach(t => t.classList.remove('active'));
                this.classList.add('active');
                
                // Show corresponding section
                const sectionId = this.dataset.section;
                document.querySelectorAll('.section').forEach(s => s.classList.remove('active'));
                document.getElementById(sectionId).classList.add('active');
            });
        });

        // WebSocket event handlers
        webSocket.onopen = function() {
            statusDot.classList.remove('disconnected');
            statusDot.classList.add('connected');
            connectionStatus.textContent = 'Connected';
            
            // Request full dashboard data
            webSocket.send(JSON.stringify({ type: 'request_full_update' }));
        };

        webSocket.onclose = function() {
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

        webSocket.onmessage = function(event) {
            const data = JSON.parse(event.data);
            
            if (data.type === 'full_update') {
                handleFullUpdate(data);
            } else if (data.type === 'component_update') {
                handleComponentUpdate(data.component);
            } else if (data.type === 'log') {
                addLogEntry(data.entry);
            } else if (data.type === 'machine_state') {
                updateMachineState(data.state);
            }
        };

        // Handle full dashboard update
        function handleFullUpdate(data) {
            // Clear all containers
            pinGrid.innerHTML = '';
            controlsGrid.innerHTML = '';
            settingsGrid.innerHTML = '';
            logContainer.innerHTML = '';
            
            // Update machine state
            if (data.machineState) {
                updateMachineState(data.machineState);
            }
            
            // Process components
            components = {};
            if (data.components) {
                data.components.forEach(component => {
                    components[component.id] = component;
                    createComponent(component);
                });
            }
            
            // Process logs
            if (data.logs) {
                data.logs.forEach(entry => {
                    addLogEntry(entry);
                });
            }
        }

        // Handle component update
        function handleComponentUpdate(component) {
            components[component.id] = component;
            
            const existingElement = document.getElementById(`component-${component.id}`);
            if (existingElement) {
                updateComponentValue(component);
            } else {
                createComponent(component);
            }
        }

        // Create a component based on its type
        function createComponent(component) {
            switch (component.type) {
                case 1: // Button
                    createButton(component);
                    break;
                case 2: // Toggle
                    createToggle(component);
                    break;
                case 3: // Slider
                    createSlider(component);
                    break;
                case 4: // Text Input
                    createTextInput(component);
                    break;
                case 5: // Select
                    createSelect(component);
                    break;
                case 6: // Pin Monitor
                    createPinMonitor(component);
                    break;
                case 7: // Machine State
                    updateMachineState(component.value);
                    break;
            }
        }

        // Update a component's value
        function updateComponentValue(component) {
            switch (component.type) {
                case 2: // Toggle
                    updateToggle(component);
                    break;
                case 3: // Slider
                    updateSlider(component);
                    break;
                case 4: // Text Input
                    updateTextInput(component);
                    break;
                case 5: // Select
                    updateSelect(component);
                    break;
                case 6: // Pin Monitor
                    updatePinMonitor(component);
                    break;
                case 7: // Machine State
                    updateMachineState(component.value);
                    break;
            }
        }

        // Component creation functions
        function createButton(component) {
            const button = document.createElement('button');
            button.className = 'control-button';
            button.id = `component-${component.id}`;
            button.textContent = component.label;
            button.onclick = function() {
                sendButtonPress(component.id);
            };
            
            controlsGrid.appendChild(button);
        }

        function createToggle(component) {
            const settingItem = document.createElement('div');
            settingItem.className = 'setting-item';
            settingItem.id = `component-${component.id}`;
            
            const label = document.createElement('div');
            label.className = 'setting-label';
            label.textContent = component.label;
            
            const toggleContainer = document.createElement('label');
            toggleContainer.className = 'toggle-switch';
            
            const input = document.createElement('input');
            input.type = 'checkbox';
            input.checked = component.value;
            input.onchange = function() {
                sendToggleChange(component.id, this.checked);
            };
            
            const slider = document.createElement('span');
            slider.className = 'toggle-slider';
            
            toggleContainer.appendChild(input);
            toggleContainer.appendChild(slider);
            
            settingItem.appendChild(label);
            settingItem.appendChild(toggleContainer);
            
            settingsGrid.appendChild(settingItem);
        }

        function createSlider(component) {
            const settingItem = document.createElement('div');
            settingItem.className = 'setting-item';
            settingItem.id = `component-${component.id}`;
            
            const label = document.createElement('div');
            label.className = 'setting-label';
            label.textContent = component.label;
            
            const sliderContainer = document.createElement('div');
            sliderContainer.className = 'slider-container';
            
            const input = document.createElement('input');
            input.type = 'range';
            input.className = 'slider-input';
            input.min = component.config.min;
            input.max = component.config.max;
            input.step = component.config.step || 1;
            input.value = component.value;
            
            const valueDisplay = document.createElement('div');
            valueDisplay.className = 'slider-value';
            valueDisplay.textContent = component.value;
            
            input.oninput = function() {
                valueDisplay.textContent = this.value;
            };
            
            input.onchange = function() {
                sendSliderChange(component.id, parseInt(this.value));
            };
            
            sliderContainer.appendChild(input);
            sliderContainer.appendChild(valueDisplay);
            
            settingItem.appendChild(label);
            settingItem.appendChild(sliderContainer);
            
            settingsGrid.appendChild(settingItem);
        }

        function createTextInput(component) {
            const settingItem = document.createElement('div');
            settingItem.className = 'setting-item';
            settingItem.id = `component-${component.id}`;
            
            const label = document.createElement('div');
            label.className = 'setting-label';
            label.textContent = component.label;
            
            const input = document.createElement('input');
            input.type = 'text';
            input.className = 'text-input';
            input.value = component.value;
            
            input.onchange = function() {
                sendTextInputChange(component.id, this.value);
            };
            
            settingItem.appendChild(label);
            settingItem.appendChild(input);
            
            settingsGrid.appendChild(settingItem);
        }

        function createSelect(component) {
            const settingItem = document.createElement('div');
            settingItem.className = 'setting-item';
            settingItem.id = `component-${component.id}`;
            
            const label = document.createElement('div');
            label.className = 'setting-label';
            label.textContent = component.label;
            
            const select = document.createElement('select');
            select.className = 'select-input';
            
            component.config.options.forEach(option => {
                const optionElement = document.createElement('option');
                optionElement.value = option;
                optionElement.textContent = option;
                if (option === component.value) {
                    optionElement.selected = true;
                }
                select.appendChild(optionElement);
            });
            
            select.onchange = function() {
                sendSelectChange(component.id, this.value);
            };
            
            settingItem.appendChild(label);
            settingItem.appendChild(select);
            
            settingsGrid.appendChild(settingItem);
        }

        function createPinMonitor(component) {
            const pinMonitor = document.createElement('div');
            pinMonitor.className = 'pin-monitor';
            pinMonitor.id = `component-${component.id}`;
            
            const label = document.createElement('div');
            label.className = 'pin-label';
            label.textContent = component.label;
            
            const isAnalog = component.max > 1;
            
            if (!isAnalog) {
                // Digital pin
                const indicator = document.createElement('div');
                indicator.className = `pin-digital ${component.value ? 'pin-on' : 'pin-off'}`;
                pinMonitor.appendChild(indicator);
            }
            
            const value = document.createElement('div');
            value.className = 'pin-value';
            value.textContent = component.value;
            
            if (isAnalog) {
                // Analog pin with progress bar
                const bar = document.createElement('div');
                bar.className = 'pin-bar';
                
                const progress = document.createElement('div');
                progress.className = 'pin-progress';
                const percentage = ((component.value - component.min) / (component.max - component.min)) * 100;
                progress.style.width = `${percentage}%`;
                
                bar.appendChild(progress);
                pinMonitor.appendChild(bar);
            }
            
            pinMonitor.appendChild(label);
            pinMonitor.appendChild(value);
            
            pinGrid.appendChild(pinMonitor);
        }

        // Component update functions
        function updateToggle(component) {
            const settingItem = document.getElementById(`component-${component.id}`);
            if (settingItem) {
                const input = settingItem.querySelector('input[type="checkbox"]');
                if (input) {
                    input.checked = component.value;
                }
            }
        }

        function updateSlider(component) {
            const settingItem = document.getElementById(`component-${component.id}`);
            if (settingItem) {
                const input = settingItem.querySelector('input[type="range"]');
                const valueDisplay = settingItem.querySelector('.slider-value');
                
                if (input && valueDisplay) {
                    input.value = component.value;
                    valueDisplay.textContent = component.value;
                }
            }
        }

        function updateTextInput(component) {
            const settingItem = document.getElementById(`component-${component.id}`);
            if (settingItem) {
                const input = settingItem.querySelector('input[type="text"]');
                if (input) {
                    input.value = component.value;
                }
            }
        }

        function updateSelect(component) {
            const settingItem = document.getElementById(`component-${component.id}`);
            if (settingItem) {
                const select = settingItem.querySelector('select');
                if (select) {
                    select.value = component.value;
                }
            }
        }

        function updatePinMonitor(component) {
            const pinMonitor = document.getElementById(`component-${component.id}`);
            if (pinMonitor) {
                const valueElement = pinMonitor.querySelector('.pin-value');
                if (valueElement) {
                    valueElement.textContent = component.value;
                }
                
                const isAnalog = component.max > 1;
                
                if (isAnalog) {
                    const progressElement = pinMonitor.querySelector('.pin-progress');
                    if (progressElement) {
                        const percentage = ((component.value - component.min) / (component.max - component.min)) * 100;
                        progressElement.style.width = `${percentage}%`;
                    }
                } else {
                    const indicator = pinMonitor.querySelector('.pin-digital');
                    if (indicator) {
                        if (component.value) {
                            indicator.classList.add('pin-on');
                            indicator.classList.remove('pin-off');
                        } else {
                            indicator.classList.add('pin-off');
                            indicator.classList.remove('pin-on');
                        }
                    }
                }
            }
        }

        function updateMachineState(state) {
            machineState.textContent = state;
        }

        // Add a log entry to the log container
        function addLogEntry(entry) {
            const logEntry = document.createElement('div');
            logEntry.className = `log-entry log-${entry.level === 0 ? 'info' : entry.level === 1 ? 'warning' : entry.level === 2 ? 'error' : 'debug'}`;
            
            const time = new Date(entry.timestamp).toLocaleTimeString();
            logEntry.textContent = `[${time}] ${entry.message}`;
            
            logContainer.appendChild(logEntry);
            logContainer.scrollTop = logContainer.scrollHeight;
            
            // Limit the number of log entries to prevent memory issues
            while (logContainer.children.length > 100) {
                logContainer.removeChild(logContainer.firstChild);
            }
        }

        // Send messages to the server
        function sendButtonPress(id) {
            webSocket.send(JSON.stringify({
                type: 'button_press',
                id: id
            }));
        }

        function sendToggleChange(id, state) {
            webSocket.send(JSON.stringify({
                type: 'toggle_change',
                id: id,
                value: state
            }));
        }

        function sendSliderChange(id, value) {
            webSocket.send(JSON.stringify({
                type: 'slider_change',
                id: id,
                value: value
            }));
        }

        function sendTextInputChange(id, value) {
            webSocket.send(JSON.stringify({
                type: 'text_input_change',
                id: id,
                value: value
            }));
        }

        function sendSelectChange(id, value) {
            webSocket.send(JSON.stringify({
                type: 'select_change',
                id: id,
                value: value
            }));
        }
    </script>
</body>
</html>
)rawliteral";

#endif  // DashboardHTML_h