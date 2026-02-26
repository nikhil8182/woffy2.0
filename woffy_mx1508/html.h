/*
 * Woffy 2.0 — HTML UI with Connection Status
 */
#ifndef HTML_H
#define HTML_H

const char PROGMEM CONTROL_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Woffy 2.0</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif; 
           background: #0a0a0f; color: #e0e0e0; min-height: 100vh; }
    .container { max-width: 500px; margin: 0 auto; padding: 20px; }
    
    /* Status Bar */
    .status-bar { display: flex; gap: 10px; justify-content: center; margin-bottom: 20px; flex-wrap: wrap; }
    .status-badge { 
      padding: 8px 16px; border-radius: 20px; font-size: 12px; font-weight: 600; 
      display: flex; align-items: center; gap: 6px;
    }
    .status-dot { width: 8px; height: 8px; border-radius: 50%; }
    .wifi-connected { background: #1a3a2a; color: #4ade80; }
    .wifi-disconnected { background: #3a1a1a; color: #f87171; }
    .mqtt-connected { background: #1a2a3a; color: #60a5fa; }
    .mqtt-disconnected { background: #3a2a1a; color: #fb923c; }
    .dot-green { background: #4ade80; }
    .dot-red { background: #f87171; }
    .dot-orange { background: #fb923c; }
    
    /* Header */
    h1 { text-align: center; color: #00d4ff; font-size: 28px; margin-bottom: 5px; }
    .subtitle { text-align: center; color: #666; font-size: 12px; margin-bottom: 20px; }
    
    /* Info Panel */
    .info-panel { 
      background: #12121a; border-radius: 12px; padding: 15px; margin-bottom: 20px;
      display: grid; grid-template-columns: 1fr 1fr; gap: 10px;
    }
    .info-item { text-align: center; }
    .info-label { font-size: 10px; color: #666; text-transform: uppercase; }
    .info-value { font-size: 14px; color: #fff; margin-top: 2px; }
    
    /* Controls Grid */
    .controls { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; margin-bottom: 20px; }
    .btn { 
      aspect-ratio: 1; border: none; border-radius: 16px; font-size: 28px; cursor: pointer;
      transition: all 0.15s ease; display: flex; align-items: center; justify-content: center;
    }
    .btn:active { transform: scale(0.95); }
    .btn-up { background: linear-gradient(145deg, #2a2a3a, #1a1a2a); color: #00d4ff; grid-column: 2; }
    .btn-left { background: linear-gradient(145deg, #2a2a3a, #1a1a2a); color: #fbbf24; }
    .btn-right { background: linear-gradient(145deg, #2a2a3a, #1a1a2a); color: #fbbf24; }
    .btn-down { background: linear-gradient(145deg, #2a2a3a, #1a1a2a); color: #f87171; grid-column: 2; }
    .btn-stop { background: linear-gradient(145deg, #3a1a1a, #2a0a0a); color: #f87171; grid-column: 2; }
    
    /* Speed Slider */
    .speed-panel { 
      background: #12121a; border-radius: 12px; padding: 20px; margin-bottom: 20px; 
    }
    .speed-label { display: flex; justify-content: space-between; margin-bottom: 10px; }
    .speed-value { color: #00d4ff; font-weight: bold; }
    input[type="range"] {
      width: 100%; height: 8px; border-radius: 4px; background: #2a2a3a;
      -webkit-appearance: none; appearance: none;
    }
    input[type="range"]::-webkit-slider-thumb {
      -webkit-appearance: none; width: 24px; height: 24px; border-radius: 50%;
      background: #00d4ff; cursor: pointer; box-shadow: 0 0 10px #00d4ff80;
    }
    
    /* Command Input */
    .cmd-panel { display: flex; gap: 10px; margin-bottom: 20px; }
    .cmd-input { 
      flex: 1; padding: 12px; border-radius: 8px; border: 1px solid #333;
      background: #12121a; color: #fff; font-size: 14px;
    }
    .cmd-input:focus { outline: none; border-color: #00d4ff; }
    .cmd-btn { 
      padding: 12px 20px; border-radius: 8px; border: none; 
      background: #00d4ff; color: #000; font-weight: bold; cursor: pointer;
    }
    
    /* Log */
    .log-panel { 
      background: #0a0a0f; border: 1px solid #222; border-radius: 8px; 
      padding: 12px; max-height: 150px; overflow-y: auto; font-family: monospace;
    }
    .log-entry { font-size: 12px; color: #666; margin: 2px 0; }
    .log-entry:last-child { color: #00d4ff; }
    
    /* Footer */
    .footer { text-align: center; margin-top: 20px; }
    .footer a { color: #666; text-decoration: none; font-size: 12px; }
    .footer a:hover { color: #00d4ff; }
  </style>
</head>
<body>
  <div class="container">
    <!-- Status Bar -->
    <div class="status-bar">
      <div id="wifiStatus" class="status-badge wifi-disconnected">
        <span id="wifiDot" class="status-dot dot-red"></span>
        <span id="wifiText">Wi-Fi</span>
      </div>
      <div id="mqttStatus" class="status-badge mqtt-disconnected">
        <span id="mqttDot" class="status-dot dot-orange"></span>
        <span id="mqttText">MQTT</span>
      </div>
    </div>
    
    <h1>WOFFY 2.0</h1>
    <p class="subtitle">MQTT + Wi-Fi Control</p>
    
    <!-- Info Panel -->
    <div class="info-panel">
      <div class="info-item">
        <div class="info-label">SSID</div>
        <div id="ssidVal" class="info-value">-</div>
      </div>
      <div class="info-item">
        <div class="info-label">IP Address</div>
        <div id="ipVal" class="info-value">-</div>
      </div>
    </div>
    
    <!-- D-Pad Controls -->
    <div class="controls">
      <button class="btn btn-up" onmousedown="sendCmd('forward')" ontouchstart="sendCmd('forward')" onmouseup="sendCmd('stop')" onmouseleave="sendCmd('stop')" ontouchend="sendCmd('stop')">▲</button>
      <button class="btn btn-left" onmousedown="sendCmd('left')" ontouchstart="sendCmd('left')" onmouseup="sendCmd('stop')" onmouseleave="sendCmd('stop')" ontouchend="sendCmd('stop')">◀</button>
      <button class="btn btn-right" onmousedown="sendCmd('right')" ontouchstart="sendCmd('right')" onmouseup="sendCmd('stop')" onmouseleave="sendCmd('stop')" ontouchend="sendCmd('stop')">▶</button>
      <button class="btn btn-down" onmousedown="sendCmd('backward')" ontouchstart="sendCmd('backward')" onmouseup="sendCmd('stop')" onmouseleave="sendCmd('stop')" ontouchend="sendCmd('stop')">▼</button>
      <button class="btn btn-stop" onclick="sendCmd('stop')">■ STOP</button>
    </div>
    
    <!-- Speed Slider -->
    <div class="speed-panel">
      <div class="speed-label">
        <span>Motor Speed</span>
        <span id="speedVal" class="speed-value">180</span>
      </div>
      <input type="range" id="speedSlider" min="60" max="255" value="180" oninput="updateSpeed(this.value)">
    </div>
    
    <!-- Command Input -->
    <div class="cmd-panel">
      <input type="text" id="cmdInput" class="cmd-input" placeholder="Command (fwd, bwd, left, right, stop)" onkeypress="if(event.key==='Enter')sendCustom()">
      <button class="cmd-btn" onclick="sendCustom()">Send</button>
    </div>
    
    <!-- Log -->
    <div id="log" class="log-panel"></div>
    
    <div class="footer">
      <a href="/setup">Wi-Fi Setup</a> | 
      <a href="/ota">OTA Info</a>
    </div>
  </div>
  
  <script>
    let currentSpeed = 180;
    let lastCmd = 'stop';
    
    function log(msg) {
      const logEl = document.getElementById('log');
      const entry = document.createElement('div');
      entry.className = 'log-entry';
      entry.textContent = '> ' + msg;
      logEl.appendChild(entry);
      logEl.scrollTop = logEl.scrollHeight;
      if (logEl.children.length > 20) logEl.removeChild(logEl.firstChild);
    }
    
    function updateStatus() {
      fetch('/wifi/status').then(r => r.json()).then(data => {
        // Wi-Fi Status
        const wifiEl = document.getElementById('wifiStatus');
        const wifiDot = document.getElementById('wifiDot');
        const wifiText = document.getElementById('wifiText');
        
        if (data.sta_connected) {
          wifiEl.className = 'status-badge wifi-connected';
          wifiDot.className = 'status-dot dot-green';
          wifiText.textContent = data.sta_ssid;
          document.getElementById('ssidVal').textContent = data.sta_ssid;
          document.getElementById('ipVal').textContent = data.sta_ip;
        } else {
          wifiEl.className = 'status-badge wifi-disconnected';
          wifiDot.className = 'status-dot dot-red';
          wifiText.textContent = 'No Wi-Fi';
          document.getElementById('ssidVal').textContent = 'Not connected';
          document.getElementById('ipVal').textContent = '-';
        }
        
        // MQTT Status
        const mqttEl = document.getElementById('mqttStatus');
        const mqttDot = document.getElementById('mqttDot');
        const mqttText = document.getElementById('mqttText');
        
        if (data.mqtt_connected) {
          mqttEl.className = 'status-badge mqtt-connected';
          mqttDot.className = 'status-dot dot-green';
          mqttText.textContent = 'MQTT';
        } else {
          mqttEl.className = 'status-badge mqtt-disconnected';
          mqttDot.className = 'status-dot dot-red';
          mqttText.textContent = 'No MQTT';
        }
      }).catch(() => {});
    }
    
    function updateSpeed(val) {
      currentSpeed = parseInt(val);
      document.getElementById('speedVal').textContent = val;
      fetch('/cmd?cmd=speed:' + val).then(() => {});
    }
    
    function sendCmd(cmd) {
      lastCmd = cmd;
      fetch('/cmd?cmd=' + cmd).then(r => r.text()).then(t => {
        log(cmd + ' - ' + t);
      });
    }
    
    function sendCustom() {
      const input = document.getElementById('cmdInput');
      const cmd = input.value.trim();
      if (!cmd) return;
      sendCmd(cmd);
      input.value = '';
    }
    
    // Initial
    updateStatus();
    setInterval(updateStatus, 5000);
    log('Ready');
  </script>
</body>
</html>
)rawliteral";

#endif
