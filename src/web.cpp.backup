#include "web.h"
#include "config.h"
#include <Preferences.h>
#include <WiFi.h>

// Global configuration
Config runtime_config = {
  .display_mode = DISPLAY_MODE,
  .db_floor = DB_FLOOR,
  .db_normal_switchover = DB_NORMAL_SWITCHOVER,
  .db_warning_switchover = DB_WARNING_SWITCHOVER,
  .led_brightness = LED_BRIGHTNESS,
  .color_normal = 0x00FF00,
  .color_warning = 0xFFFF00,
  .color_alert = 0xFF0000,
  .decay_ms = 1500,
  .response_ms = 100
};

// Current sound level (updated from main loop)
static double current_dB = 0.0;
static unsigned long last_dB_update = 0;

// Deferred save flag (set by handler, processed by web task)
bool needs_save = false;

// Web server on port 80
WebServer web_server(80);

// Persistent storage
Preferences preferences;

// Embedded HTML UI
const char* html_ui = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Noise Light Config</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 12px;
      box-shadow: 0 10px 40px rgba(0,0,0,0.2);
      max-width: 500px;
      width: 100%;
      padding: 30px;
    }
    h1 {
      color: #333;
      margin-bottom: 30px;
      font-size: 28px;
      text-align: center;
    }
    .section {
      margin-bottom: 25px;
    }
    .section-title {
      font-size: 16px;
      font-weight: 600;
      color: #667eea;
      margin-bottom: 12px;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    label {
      display: block;
      margin-bottom: 8px;
      color: #555;
      font-size: 14px;
      font-weight: 500;
    }
    input[type="radio"] {
      margin-right: 8px;
    }
    .radio-group {
      display: flex;
      gap: 20px;
      margin-bottom: 12px;
    }
    .radio-group label {
      display: flex;
      align-items: center;
      margin: 0;
    }
    input[type="range"] {
      width: 100%;
      height: 6px;
      border-radius: 3px;
      background: #ddd;
      outline: none;
      -webkit-appearance: none;
      margin-bottom: 6px;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none;
      appearance: none;
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: #667eea;
      cursor: pointer;
      box-shadow: 0 2px 4px rgba(0,0,0,0.2);
    }
    input[type="range"]::-moz-range-thumb {
      width: 18px;
      height: 18px;
      border-radius: 50%;
      background: #667eea;
      cursor: pointer;
      border: none;
    }
    .value-display {
      text-align: right;
      font-size: 13px;
      color: #888;
      font-weight: 500;
    }
    .range-container {
      margin-bottom: 15px;
    }
    .led-preview {
      display: flex;
      gap: 6px;
      height: 40px;
      border-radius: 6px;
      overflow: hidden;
      margin: 15px 0;
      box-shadow: inset 0 2px 4px rgba(0,0,0,0.1);
    }
    .led { flex: 1; }
    .status {
      margin-top: 20px;
      padding: 12px;
      background: #f0f4ff;
      border-left: 4px solid #667eea;
      border-radius: 4px;
      font-size: 13px;
      color: #555;
    }
    .status.success {
      background: #f0fdf4;
      border-left-color: #22c55e;
      color: #166534;
    }
    .live-level {
      background: #f8f9fa;
      border-radius: 8px;
      padding: 20px;
      margin-bottom: 25px;
    }
    .level-value {
      display: flex;
      justify-content: space-between;
      align-items: baseline;
      margin-bottom: 12px;
    }
    .level-number {
      font-size: 36px;
      font-weight: 700;
      color: #333;
    }
    .level-unit {
      font-size: 14px;
      color: #999;
    }
    .level-bar-container {
      width: 100%;
      height: 24px;
      background: #e5e7eb;
      border-radius: 4px;
      overflow: hidden;
      margin-top: 12px;
    }
    .level-bar {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, #22c55e 0%, #eab308 50%, #ef4444 100%);
      transition: width 0.1s ease;
    }
    button {
      width: 100%;
      padding: 12px;
      background: #667eea;
      color: white;
      border: none;
      border-radius: 6px;
      font-size: 16px;
      font-weight: 600;
      cursor: pointer;
      margin-top: 20px;
      transition: background 0.3s;
    }
    button:hover { background: #5568d3; }
    button:active { transform: scale(0.98); }
    .color-picker {
      width: 60px;
      height: 40px;
      border: none;
      border-radius: 4px;
      cursor: pointer;
    }
    .color-group {
      display: flex;
      gap: 10px;
      align-items: center;
      margin-bottom: 10px;
    }
    .color-group label {
      width: 80px;
      margin: 0;
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🎵 Noise Light</h1>
    
    <div class="live-level">
      <div class="level-value">
        <div>
          <div class="level-number" id="live-db">--</div>
          <div class="level-unit">Current Level</div>
        </div>
      </div>
      <div class="level-bar-container">
        <div class="level-bar" id="live-bar"></div>
      </div>
    </div>
    
    <div class="section">
      <div class="section-title">Display Mode</div>
      <div class="radio-group">
        <label><input type="radio" name="mode" value="0" onchange="updatePreview()"> Traffic Light</label>
        <label><input type="radio" name="mode" value="1" onchange="updatePreview()"> VU Meter</label>
      </div>
    </div>

    <div class="section">
      <div class="section-title">LED Settings</div>
      
      <div class="range-container">
        <label>Brightness <span class="value-display" id="brightness-val">25</span></label>
        <input type="range" id="brightness-slider" min="0" max="255" step="1" value="25" onchange="updatePreview()">
      </div>
      
      <div style="margin-top: 20px;">
        <div class="color-group">
          <label>Green</label>
          <input type="color" id="color-green" value="#00FF00" class="color-picker" onchange="updatePreview()">
        </div>
        <div class="color-group">
          <label>Yellow</label>
          <input type="color" id="color-yellow" value="#FFFF00" class="color-picker" onchange="updatePreview()">
        </div>
        <div class="color-group">
          <label>Red</label>
          <input type="color" id="color-red" value="#FF0000" class="color-picker" onchange="updatePreview()">
        </div>
      </div>
    </div>

    <div class="section">
      <div class="section-title">Color Switchover Points (dB)</div>
      
      <div class="range-container">
        <div style="display: flex; justify-content: space-between; align-items: center;">
          <label style="margin-bottom: 0;">Floor Level <span class="value-display" id="floor-val">37 dB</span></label>
          <button style="width: auto; padding: 6px 12px; margin-top: 0; font-size: 12px;" onclick="setFloorToCurrent()">Current</button>
        </div>
        <input type="range" id="floor-slider" min="20" max="60" step="1" value="37" onchange="updatePreview()">
      </div>
      
      <div class="range-container">
        <label>Green→Yellow Switchover <span class="value-display" id="green-val">50 dB</span></label>
        <input type="range" id="green-slider" min="30" max="70" step="1" value="50" onchange="updatePreview()">
      </div>
      
      <div class="range-container">
        <label>Yellow→Red Switchover <span class="value-display" id="yellow-val">65 dB</span></label>
        <input type="range" id="yellow-slider" min="40" max="85" step="1" value="65" onchange="updatePreview()">
      </div>
      
      <div class="led-preview" id="preview"></div>
    </div>

    <div class="section">
      <div class="section-title">Response Timing</div>
      
      <div class="range-container">
        <label>Decay Time <span class="value-display" id="decay-val">1500 ms</span></label>
        <input type="range" id="decay-slider" min="0" max="3000" step="100" value="1500" onchange="updatePreview()">
      </div>
      
      <div class="range-container">
        <label>Response Time <span class="value-display" id="response-val">100 ms</span></label>
        <input type="range" id="response-slider" min="0" max="500" step="50" value="100" onchange="updatePreview()">
      </div>
    </div>

    <div id="status" class="status" style="display:none;"></div>
    
    <button onclick="saveConfig()">Save Configuration</button>
  </div>

  <script>
    async function updateLiveLevel() {
      try {
        const res = await fetch('/api/status');
        const data = await res.json();
        document.getElementById('live-db').textContent = data.db.toFixed(1);
        
        // Calculate bar width (37-80 dB range)
        const minDb = 37;
        const maxDb = 80;
        const normalized = Math.max(0, Math.min(1, (data.db - minDb) / (maxDb - minDb)));
        document.getElementById('live-bar').style.width = (normalized * 100) + '%';
      } catch (e) {
        console.error('Status update failed:', e);
      }
    }

    async function loadConfig() {
      try {
        const res = await fetch('/api/config');
        const data = await res.json();
        document.querySelector(`input[name="mode"][value="${data.display_mode}"]`).checked = true;
        document.getElementById('floor-slider').value = Math.round(data.db_floor);
        document.getElementById('green-slider').value = Math.round(data.db_normal_switchover);
        document.getElementById('yellow-slider').value = Math.round(data.db_warning_switchover);
        document.getElementById('brightness-slider').value = data.led_brightness;
        document.getElementById('color-green').value = '#' + ('000000' + data.color_normal.toString(16).toUpperCase()).slice(-6);
        document.getElementById('color-yellow').value = '#' + ('000000' + data.color_warning.toString(16).toUpperCase()).slice(-6);
        document.getElementById('color-red').value = '#' + ('000000' + data.color_alert.toString(16).toUpperCase()).slice(-6);
        document.getElementById('decay-slider').value = data.decay_ms;
        document.getElementById('response-slider').value = data.response_ms;
        updatePreview();
      } catch (e) {
        console.error('Failed to load config:', e);
      }
    }

    function updatePreview() {
      const brightness = parseInt(document.getElementById('brightness-slider').value);
      const floorLevel = parseInt(document.getElementById('floor-slider').value);
      const greenThresh = parseInt(document.getElementById('green-slider').value);
      const yellowThresh = parseInt(document.getElementById('yellow-slider').value);
      const decay = parseInt(document.getElementById('decay-slider').value);
      const response = parseInt(document.getElementById('response-slider').value);
      
      document.getElementById('brightness-val').textContent = brightness;
      document.getElementById('floor-val').textContent = floorLevel + ' dB';
      document.getElementById('green-val').textContent = greenThresh + ' dB';
      document.getElementById('yellow-val').textContent = yellowThresh + ' dB';
      document.getElementById('decay-val').textContent = decay + ' ms';
      document.getElementById('response-val').textContent = response + ' ms';
      
      const colors = [];
      for (let i = 0; i < 13; i++) {
        const dbAtLED = 37 + (i / 12) * 43; // 37-80 dB range
        let color;
        if (dbAtLED < greenThresh) {
          color = '#22c55e'; // Green
        } else if (dbAtLED < yellowThresh) {
          color = '#eab308'; // Yellow
        } else {
          color = '#ef4444'; // Red
        }
        colors.push(`<div class="led" style="background:${color}"></div>`);
      }
      document.getElementById('preview').innerHTML = colors.join('');
    }

    function setFloorToCurrent() {
      const currentDb = parseFloat(document.getElementById('live-db').textContent);
      if (!isNaN(currentDb)) {
        document.getElementById('floor-slider').value = Math.round(currentDb);
        updatePreview();
      }
    }

    function hexToInt(hex) {
      return parseInt(hex.substring(1), 16);
    }

    async function saveConfig() {
      const mode = document.querySelector('input[name="mode"]:checked').value;
      const floor = parseInt(document.getElementById('floor-slider').value);
      const green = parseInt(document.getElementById('green-slider').value);
      const yellow = parseInt(document.getElementById('yellow-slider').value);
      const brightness = parseInt(document.getElementById('brightness-slider').value);
      const colorGreen = hexToInt(document.getElementById('color-green').value);
      const colorYellow = hexToInt(document.getElementById('color-yellow').value);
      const colorRed = hexToInt(document.getElementById('color-red').value);
      const decay = parseInt(document.getElementById('decay-slider').value);
      const response = parseInt(document.getElementById('response-slider').value);
      
      try {
        const res = await fetch('/api/config', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            display_mode: parseInt(mode),
            db_floor: floor,
            db_normal_switchover: green,
            db_warning_switchover: yellow,
            led_brightness: brightness,
            color_normal: colorGreen,
            color_warning: colorYellow,
            color_alert: colorRed,
            decay_ms: decay,
            response_ms: response
          })
        });
        
        const status = document.getElementById('status');
        if (res.ok) {
          status.className = 'status success';
          status.textContent = '✓ Configuration saved successfully!';
        } else {
          status.className = 'status';
          status.textContent = '✗ Failed to save configuration';
        }
        status.style.display = 'block';
        setTimeout(() => { status.style.display = 'none'; }, 3000);
      } catch (e) {
        console.error('Save failed:', e);
      }
    }

    window.onload = function() {
      loadConfig();
      updateLiveLevel();  // Initial update
      setInterval(updateLiveLevel, 200);  // Update every 200ms
    };
  </script>
</body>
</html>
)rawliteral";

// Load config from persistent storage
void web_load_config() {
  preferences.begin("noiselight", true);  // readonly
  runtime_config.display_mode = preferences.getInt("mode", DISPLAY_MODE);
  runtime_config.db_floor = preferences.getFloat("floor", DB_FLOOR);
  runtime_config.db_normal_switchover = preferences.getFloat("normal", DB_NORMAL_SWITCHOVER);
  runtime_config.db_warning_switchover = preferences.getFloat("warning", DB_WARNING_SWITCHOVER);
  runtime_config.led_brightness = preferences.getUChar("brightness", LED_BRIGHTNESS);
  runtime_config.color_normal = preferences.getUInt("col_normal", 0x00FF00);
  runtime_config.color_warning = preferences.getUInt("col_warning", 0xFFFF00);
  runtime_config.color_alert = preferences.getUInt("col_alert", 0xFF0000);
  runtime_config.decay_ms = preferences.getUShort("decay_ms", 1500);
  runtime_config.response_ms = preferences.getUShort("response_ms", 100);
  preferences.end();
  
  log_i("Config loaded: mode=%d decay=%dms response=%dms", 
    runtime_config.display_mode,
    runtime_config.decay_ms,
    runtime_config.response_ms);
}

// Save config to persistent storage
void web_save_config() {
  preferences.begin("noiselight", false);  // readwrite
  preferences.putInt("mode", runtime_config.display_mode);
  preferences.putFloat("floor", runtime_config.db_floor);
  preferences.putFloat("normal", runtime_config.db_normal_switchover);
  preferences.putFloat("warning", runtime_config.db_warning_switchover);
  preferences.putUChar("brightness", runtime_config.led_brightness);
  preferences.putUInt("col_normal", runtime_config.color_normal);
  preferences.putUInt("col_warning", runtime_config.color_warning);
  preferences.putUInt("col_alert", runtime_config.color_alert);
  preferences.putUShort("decay_ms", runtime_config.decay_ms);
  preferences.putUShort("response_ms", runtime_config.response_ms);
  preferences.end();
  
  const char* mode_str = (runtime_config.display_mode == 0) ? "TRAFFIC_LIGHT" : "VU_METER";
  log_i("=== CONFIG SAVED ===");
  log_i("Mode: %s", mode_str);
  log_i("Floor: %.1f dB", runtime_config.db_floor);
  log_i("Switchovers: normal=%.1f dB, warning=%.1f dB", 
    runtime_config.db_normal_switchover,
    runtime_config.db_warning_switchover);
  log_i("LED Brightness: %d", runtime_config.led_brightness);
  log_i("Colors: normal=0x%06X, warning=0x%06X, alert=0x%06X",
    runtime_config.color_normal,
    runtime_config.color_warning,
    runtime_config.color_alert);
  log_i("Timing: decay=%dms, response=%dms",
    runtime_config.decay_ms,
    runtime_config.response_ms);
}

// Serve main HTML page
void web_handle_root() {
  web_server.send(200, "text/html", html_ui);
}

// GET /api/config - return current config
void web_handle_api_get() {
  char json[600];
  snprintf(json, sizeof(json),
    "{\"display_mode\":%d,\"db_floor\":%.1f,\"db_normal_switchover\":%.1f,\"db_warning_switchover\":%.1f,\"led_brightness\":%d,\"color_normal\":%u,\"color_warning\":%u,\"color_alert\":%u,\"decay_ms\":%d,\"response_ms\":%d}",
    runtime_config.display_mode,
    runtime_config.db_floor,
    runtime_config.db_normal_switchover,
    runtime_config.db_warning_switchover,
    runtime_config.led_brightness,
    runtime_config.color_normal,
    runtime_config.color_warning,
    runtime_config.color_alert,
    runtime_config.decay_ms,
    runtime_config.response_ms);
  web_server.send(200, "application/json", json);
}

// POST /api/config - update config
void web_handle_api_set() {
  log_i("[WEB] Config update received");
  if (!web_server.hasArg("plain")) {
    web_server.send(400, "application/json", "{\"error\":\"No body\"}");
    return;
  }

  String body = web_server.arg("plain");
  log_i("Received JSON: %s", body.c_str());
  
  // Simple JSON parsing (avoid ArduinoJson for small config)
  int mode_pos = body.indexOf("\"display_mode\":");
  if (mode_pos >= 0) {
    runtime_config.display_mode = body.substring(mode_pos + 15, mode_pos + 16).toInt();
  }
  
  int floor_pos = body.indexOf("\"db_floor\":");
  if (floor_pos >= 0) {
    int end = body.indexOf(",", floor_pos);
    if (end < 0) end = body.indexOf("}", floor_pos);
    String val = body.substring(floor_pos + 11, end);
    runtime_config.db_floor = val.toFloat();
    log_i("Parsed floor: '%s' = %.1f", val.c_str(), runtime_config.db_floor);
  }
  
  int green_pos = body.indexOf("\"db_normal_switchover\":");
  if (green_pos >= 0) {
    int end = body.indexOf(",", green_pos);
    if (end < 0) end = body.indexOf("}", green_pos);
    String val = body.substring(green_pos + 23, end);
    val.trim();
    runtime_config.db_normal_switchover = val.toFloat();
    log_i("Parsed normal: '%s' = %.1f", val.c_str(), runtime_config.db_normal_switchover);
  }
  
  int yellow_pos = body.indexOf("\"db_warning_switchover\":");
  if (yellow_pos >= 0) {
    int end = body.indexOf(",", yellow_pos);
    if (end < 0) end = body.indexOf("}", yellow_pos);
    String val = body.substring(yellow_pos + 24, end);
    val.trim();
    runtime_config.db_warning_switchover = val.toFloat();
    log_i("Parsed warning: '%s' = %.1f", val.c_str(), runtime_config.db_warning_switchover);
  }
  
  int bright_pos = body.indexOf("\"led_brightness\":");
  if (bright_pos >= 0) {
    int end = body.indexOf(",", bright_pos);
    if (end < 0) end = body.indexOf("}", bright_pos);
    String val = body.substring(bright_pos + 17, end);
    runtime_config.led_brightness = val.toInt();
  }
  
  int col_green_pos = body.indexOf("\"color_normal\":");
  if (col_green_pos >= 0) {
    int start = col_green_pos + 15;  // Skip past "color_normal":
    int end = body.indexOf(",", col_green_pos);
    if (end < 0) end = body.indexOf("}", col_green_pos);
    String val = body.substring(start, end);
    val.trim();
    uint32_t parsed = strtoul(val.c_str(), NULL, 10);
    if (parsed > 0) runtime_config.color_normal = parsed;
  }
  
  int col_yellow_pos = body.indexOf("\"color_warning\":");
  if (col_yellow_pos >= 0) {
    int start = col_yellow_pos + 16;  // Skip past "color_warning":
    int end = body.indexOf(",", col_yellow_pos);
    if (end < 0) end = body.indexOf("}", col_yellow_pos);
    String val = body.substring(start, end);
    val.trim();
    uint32_t parsed = strtoul(val.c_str(), NULL, 10);
    if (parsed > 0) runtime_config.color_warning = parsed;
  }
  
  int col_red_pos = body.indexOf("\"color_alert\":");
  if (col_red_pos >= 0) {
    int start = col_red_pos + 14;  // Skip past "color_alert":
    int end = body.indexOf(",", col_red_pos);
    if (end < 0) end = body.indexOf("}", col_red_pos);
    String val = body.substring(start, end);
    val.trim();
    uint32_t parsed = strtoul(val.c_str(), NULL, 10);
    if (parsed > 0) runtime_config.color_alert = parsed;
  }
  
  int decay_pos = body.indexOf("\"decay_ms\":");
  if (decay_pos >= 0) {
    int end = body.indexOf(",", decay_pos);
    if (end < 0) end = body.indexOf("}", decay_pos);
    String val = body.substring(decay_pos + 11, end);
    runtime_config.decay_ms = val.toInt();
  }
  
  int response_pos = body.indexOf("\"response_ms\":");
  if (response_pos >= 0) {
    int end = body.indexOf(",", response_pos);
    if (end < 0) end = body.indexOf("}", response_pos);
    String val = body.substring(response_pos + 14, end);
    runtime_config.response_ms = val.toInt();
  }

  // Send response IMMEDIATELY (don't block on NVS writes)
  web_server.send(200, "application/json", "{\"status\":\"ok\"}");
  
  // Set flag to save later (outside of handler to avoid blocking)
  needs_save = true;
}

// GET /api/status - return current sound level
void web_handle_api_status() {
  char json[64];
  snprintf(json, sizeof(json), "{\"db\":%.1f}", current_dB);
  web_server.send(200, "application/json", json);
}

// Update current dB level from main loop
void web_update_level(double dB_current) {
  current_dB = dB_current;
  last_dB_update = millis();
}

// 404 handler
void web_handle_not_found() {
  web_server.send(404, "text/plain", "Not Found");
}

// Initialize web server
void web_init() {
  log_i("[WEB] Initializing web server...");
  
  // Start WiFi in AP mode
  WiFi.mode(WIFI_AP);
  log_i("[WEB] WiFi mode set to AP");
  
  WiFi.softAP("NoiseLight", "12345678");  // SSID, Password
  log_i("[WEB] WiFi AP configured");
  
  IPAddress ap_ip = WiFi.softAPIP();
  log_i("[WEB] WiFi AP Started: http://%s", ap_ip.toString().c_str());
  
  // Setup web server routes
  log_i("[WEB] Registering routes...");
  web_server.on("/", HTTP_GET, web_handle_root);
  log_i("[WEB] Route / registered");
  
  web_server.on("/api/config", HTTP_GET, web_handle_api_get);
  log_i("[WEB] Route /api/config GET registered");
  
  web_server.on("/api/config", HTTP_POST, web_handle_api_set);
  log_i("[WEB] Route /api/config POST registered");
  
  web_server.on("/api/status", HTTP_GET, web_handle_api_status);
  log_i("[WEB] Route /api/status registered");
  
  web_server.onNotFound(web_handle_not_found);
  log_i("[WEB] 404 handler registered");
  
  web_server.begin();
  log_i("[WEB] Web server started on port 80");
  
  // Load config from storage
  log_i("[WEB] Loading config from NVS...");
  web_load_config();
  log_i("[WEB] Config loaded");
}
