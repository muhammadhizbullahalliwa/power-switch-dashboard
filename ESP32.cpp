// ========== POWER SWITCH SYSTEM V3 ==========
// Developer: mh_alliwa (Telegram: @mh_alliwa, Instagram: @mh.alliwa)
// Sistem Dual Source dengan 3 Web Interface
// Versi: 3.0 - Advanced Real-time Monitoring & Control

// ========== CONFIGURASI ==========
#define BLYNK_TEMPLATE_ID "TMPL6mhCLY2HX"
#define BLYNK_TEMPLATE_NAME "Power Switch V3"
#define BLYNK_AUTH_TOKEN "9V0U-QtNPYRO63Q-oR26CalZ1CB7fErt"

// ========== LIBRARY ==========
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <BlynkSimpleEsp32.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>

// ========== FIX LCD LIBRARY COMPATIBILITY ==========
#if defined(ESP32)
  #define SDA_PIN 21
  #define SCL_PIN 22
  #define LCD_I2C_ADDRESS 0x27
#else
  #define SDA_PIN A4
  #define SCL_PIN A5
#endif

// ========== PIN DEFINITIONS ==========
#define PIN_ZMPT1 34
#define PIN_ZMPT2 35
#define PIN_ACS 32
#define RELAY1_PIN 25
#define RELAY2_PIN 26
#define BUZZER_PIN 27
#define LED_STATUS 2
#define BUTTON_MANUAL 33
#define TEMP_SENSOR_PIN 39

// ========== WIFI & TELEGRAM ==========
char ssid[] = "KingFinix";
char pass[] = "";
#define BOT_TOKEN "8598852476:AAFsvYnX94UvAN6uSqgr1rFWP217YBGJMHg"
#define CHAT_ID "6293340373"

// ========== WEB SERVER ==========
#define WEB_PORT 80
WebServer server(WEB_PORT);

// ========== NTP CLIENT ==========
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "id.pool.ntp.org", 25200, 60000);

// ========== GITHUB PAGES CONFIG ==========
const char* githubUsername = "mh-alliwa";
const char* githubRepo = "power-switch-dashboard";

// ========== SENSOR CONFIG ==========
typedef struct {
  float voltage1;
  float voltage2;
  float current;
  float power;
  float energy;
  float frequency1;
  float frequency2;
  float powerFactor;
  float temperature;
  uint32_t timestamp;
} SensorData;

typedef struct {
  int zeroV1;
  int zeroV2;
  int zeroI;
  float calibrationFactorV1;
  float calibrationFactorV2;
  float calibrationFactorI;
  float voltageOffsetV1;
  float voltageOffsetV2;
} OptimizationData;

// ========== SYSTEM OBJECTS ==========
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 16, 2);
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
BlynkTimer timer;
Preferences preferences;

// ========== GLOBAL VARIABLES ==========
SensorData sensors = {0};
OptimizationData optim = {2048, 2048, 2048, 1.0, 1.0, 1.0, 0.0, 0.0};

enum SystemState {
  STATE_OFF,
  STATE_SOURCE1_ACTIVE,
  STATE_SOURCE2_ACTIVE,
  STATE_SWITCHING,
  STATE_FAULT,
  STATE_OPTIMIZATION,
  STATE_UPDATE
};

SystemState systemState = STATE_OFF;
bool autoMode = true;
bool maintenanceMode = false;
bool notificationEnabled = true;
bool dataLogging = true;
bool cloudSync = false;
bool otaEnabled = true;

// Energy Management
float totalEnergy = 0;
float dailyEnergy = 0;
float monthlyEnergy = 0;
uint32_t lastEnergyUpdate = 0;
uint32_t lastDailyReset = 0;
uint32_t lastMonthlyReset = 0;

// Statistics
struct {
  uint32_t totalUptime;
  uint32_t source1Uptime;
  uint32_t source2Uptime;
  uint32_t switchCount;
  uint32_t faultCount;
  uint32_t lastSwitchTime;
  uint32_t startupTime;
} stats = {0};

// Real-time data buffers
#define DATA_BUFFER_SIZE 100
float voltageHistory1[DATA_BUFFER_SIZE];
float voltageHistory2[DATA_BUFFER_SIZE];
float currentHistory[DATA_BUFFER_SIZE];
float powerHistory[DATA_BUFFER_SIZE];
int historyIndex = 0;

// System Logs
struct SystemLog {
  uint32_t timestamp;
  String level;
  String source;
  String message;
};

#define MAX_LOGS 100
SystemLog systemLogs[MAX_LOGS];
int logIndex = 0;

// Alert thresholds
float voltageLowThreshold = 170.0;
float voltageHighThreshold = 250.0;
float currentHighThreshold = 20.0;
float temperatureThreshold = 60.0;

// ========== FUNCTION PROTOTYPES ==========
void sendTelegramAlert(String message);
String getStateText();
void emergencyShutdown(String reason);
void addSystemLog(String level, String source, String message);
void handleTelegramMessages();
void updateLCD();
void checkSafety();
void performAutoSwitch();
void saveSystemSettings();
void loadSystemSettings();
float readTemperature();
String getTimeString();
String getDateString();
void initWebServer();
void performOptimization();

// ========== WEB PAGES ==========
const char* basicWebPage = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>⚡ Power Switch V3 - Control Panel</title>
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        :root {
            --primary: #4361ee;
            --secondary: #3a0ca3;
            --success: #4cc9f0;
            --danger: #f72585;
            --warning: #f8961e;
            --dark: #1a1a2e;
            --light: #f8f9fa;
            --gradient: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
        }
        
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: var(--gradient);
            color: var(--light);
            min-height: 100vh;
            padding: 20px;
            background-attachment: fixed;
        }
        
        .glass {
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border-radius: 20px;
            border: 1px solid rgba(255, 255, 255, 0.2);
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.1);
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        
        .header {
            text-align: center;
            padding: 30px 0;
            margin-bottom: 30px;
        }
        
        .logo {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .logo-icon {
            font-size: 3rem;
            color: #4cc9f0;
            animation: pulse 2s infinite;
        }
        
        @keyframes pulse {
            0%, 100% { transform: scale(1); }
            50% { transform: scale(1.1); }
        }
        
        .logo-text {
            font-size: 2.8rem;
            font-weight: 800;
            background: linear-gradient(to right, #4cc9f0, #f72585);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-shadow: 0 2px 10px rgba(0, 0, 0, 0.2);
        }
        
        .tagline {
            font-size: 1.2rem;
            opacity: 0.9;
            margin-bottom: 10px;
        }
        
        .developer {
            font-size: 0.9rem;
            opacity: 0.7;
            margin-top: 10px;
        }
        
        .status-badge {
            display: inline-block;
            padding: 12px 25px;
            border-radius: 50px;
            font-weight: bold;
            font-size: 1.1rem;
            letter-spacing: 1px;
            margin: 15px 0;
            transition: all 0.3s;
        }
        
        .status-online { 
            background: linear-gradient(135deg, #4cc9f0, #4361ee);
            box-shadow: 0 4px 15px rgba(67, 97, 238, 0.4);
        }
        
        .status-offline { 
            background: linear-gradient(135deg, #f72585, #b5179e);
            box-shadow: 0 4px 15px rgba(247, 37, 133, 0.4);
        }
        
        .status-warning { 
            background: linear-gradient(135deg, #f8961e, #f3722c);
            box-shadow: 0 4px 15px rgba(248, 150, 30, 0.4);
        }
        
        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            gap: 25px;
            margin-bottom: 30px;
        }
        
        .card {
            padding: 30px;
            transition: transform 0.3s, box-shadow 0.3s;
        }
        
        .card:hover {
            transform: translateY(-5px);
            box-shadow: 0 15px 35px rgba(0, 0, 0, 0.2);
        }
        
        .card-header {
            display: flex;
            align-items: center;
            gap: 15px;
            margin-bottom: 25px;
            padding-bottom: 15px;
            border-bottom: 2px solid rgba(255, 255, 255, 0.1);
        }
        
        .card-icon {
            font-size: 2rem;
            color: #4cc9f0;
        }
        
        .card-title {
            font-size: 1.5rem;
            font-weight: 600;
        }
        
        .sensor-value {
            font-size: 3.5rem;
            font-weight: 800;
            text-align: center;
            margin: 20px 0;
            text-shadow: 0 2px 10px rgba(0, 0, 0, 0.3);
            background: linear-gradient(to right, #ffffff, #4cc9f0);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
        }
        
        .sensor-label {
            text-align: center;
            font-size: 1rem;
            opacity: 0.8;
            margin-bottom: 15px;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .quality-indicator {
            text-align: center;
            padding: 10px;
            border-radius: 10px;
            margin-top: 15px;
            font-weight: bold;
        }
        
        .quality-excellent { background: rgba(76, 201, 240, 0.2); }
        .quality-good { background: rgba(67, 97, 238, 0.2); }
        .quality-warning { background: rgba(248, 150, 30, 0.2); }
        .quality-danger { background: rgba(247, 37, 133, 0.2); }
        
        .controls-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
            margin-top: 25px;
        }
        
        .btn {
            padding: 18px;
            border: none;
            border-radius: 15px;
            font-size: 1.1rem;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.3s;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 10px;
            position: relative;
            overflow: hidden;
        }
        
        .btn::before {
            content: '';
            position: absolute;
            top: 0;
            left: -100%;
            width: 100%;
            height: 100%;
            background: linear-gradient(90deg, transparent, rgba(255,255,255,0.2), transparent);
            transition: 0.5s;
        }
        
        .btn:hover::before {
            left: 100%;
        }
        
        .btn-primary { 
            background: linear-gradient(135deg, #4361ee, #3a0ca3);
            color: white;
        }
        
        .btn-success { 
            background: linear-gradient(135deg, #4cc9f0, #4895ef);
            color: white;
        }
        
        .btn-danger { 
            background: linear-gradient(135deg, #f72585, #b5179e);
            color: white;
        }
        
        .btn-warning { 
            background: linear-gradient(135deg, #f8961e, #f3722c);
            color: white;
        }
        
        .btn-info { 
            background: linear-gradient(135deg, #7209b7, #560bad);
            color: white;
        }
        
        .btn:hover {
            transform: translateY(-3px);
            box-shadow: 0 10px 20px rgba(0, 0, 0, 0.2);
        }
        
        .btn:active {
            transform: translateY(-1px);
        }
        
        .system-info {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin: 20px 0;
        }
        
        .info-item {
            padding: 15px;
            background: rgba(255, 255, 255, 0.05);
            border-radius: 10px;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .info-label {
            font-weight: 600;
            opacity: 0.9;
        }
        
        .info-value {
            font-weight: bold;
            color: #4cc9f0;
        }
        
        .chart-container {
            height: 350px;
            margin-top: 25px;
            padding: 20px;
        }
        
        .action-buttons {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-top: 30px;
        }
        
        .footer {
            text-align: center;
            padding: 30px 0;
            margin-top: 40px;
            border-top: 1px solid rgba(255, 255, 255, 0.1);
            font-size: 0.9rem;
            opacity: 0.7;
        }
        
        .notification {
            position: fixed;
            top: 20px;
            right: 20px;
            padding: 15px 25px;
            border-radius: 10px;
            background: rgba(76, 201, 240, 0.9);
            color: white;
            font-weight: bold;
            z-index: 1000;
            transform: translateX(400px);
            transition: transform 0.3s;
            box-shadow: 0 5px 20px rgba(0, 0, 0, 0.2);
        }
        
        .notification.show {
            transform: translateX(0);
        }
        
        .notification.error { background: rgba(247, 37, 133, 0.9); }
        .notification.warning { background: rgba(248, 150, 30, 0.9); }
        .notification.success { background: rgba(67, 97, 238, 0.9); }
        
        @media (max-width: 768px) {
            .dashboard-grid {
                grid-template-columns: 1fr;
            }
            
            .sensor-value {
                font-size: 2.8rem;
            }
            
            .logo-text {
                font-size: 2rem;
            }
            
            .card {
                padding: 20px;
            }
        }
        
        .ripple {
            position: relative;
            overflow: hidden;
        }
        
        .ripple:after {
            content: "";
            position: absolute;
            top: 50%;
            left: 50%;
            width: 5px;
            height: 5px;
            background: rgba(255, 255, 255, 0.5);
            opacity: 0;
            border-radius: 100%;
            transform: scale(1, 1) translate(-50%);
            transform-origin: 50% 50%;
        }
        
        .ripple:focus:after {
            animation: ripple 1s ease-out;
        }
        
        @keyframes ripple {
            0% {
                transform: scale(0, 0);
                opacity: 0.5;
            }
            100% {
                transform: scale(100, 100);
                opacity: 0;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <div class="header">
            <div class="logo">
                <i class="fas fa-bolt logo-icon"></i>
                <div class="logo-text">POWER SWITCH V3</div>
            </div>
            <div class="tagline">Advanced Dual Source Power Management System</div>
            <div class="status-badge" id="system-status">CONNECTING...</div>
            <div class="developer">
                <i class="fas fa-code"></i> Developer: mh_alliwa | 
                <i class="fab fa-telegram"></i> Telegram: @mh_alliwa | 
                <i class="fab fa-instagram"></i> Instagram: @mh.alliwa
            </div>
        </div>
        
        <!-- Main Dashboard -->
        <div class="dashboard-grid">
            <!-- Voltage Monitoring -->
            <div class="card glass">
                <div class="card-header">
                    <i class="fas fa-bolt card-icon"></i>
                    <div class="card-title">Voltage Monitoring</div>
                </div>
                <div class="sensor-value" id="voltage1">0.0 V</div>
                <div class="sensor-label">Source 1</div>
                
                <div class="sensor-value" id="voltage2">0.0 V</div>
                <div class="sensor-label">Source 2</div>
                
                <div class="quality-indicator" id="voltage-quality">
                    <i class="fas fa-spinner fa-spin"></i> Checking...
                </div>
            </div>
            
            <!-- Power Analytics -->
            <div class="card glass">
                <div class="card-header">
                    <i class="fas fa-chart-line card-icon"></i>
                    <div class="card-title">Power Analytics</div>
                </div>
                <div class="sensor-value" id="current">0.000 A</div>
                <div class="sensor-label">Current</div>
                
                <div class="sensor-value" id="power">0.0 W</div>
                <div class="sensor-label">Power</div>
                
                <div class="sensor-value" id="energy">0.000 kWh</div>
                <div class="sensor-label">Total Energy</div>
            </div>
            
            <!-- System Control -->
            <div class="card glass">
                <div class="card-header">
                    <i class="fas fa-cogs card-icon"></i>
                    <div class="card-title">System Control</div>
                </div>
                
                <div class="system-info">
                    <div class="info-item">
                        <span class="info-label">Mode:</span>
                        <span class="info-value" id="mode-status">AUTO</span>
                    </div>
                    <div class="info-item">
                        <span class="info-label">Maintenance:</span>
                        <span class="info-value" id="maintenance-status">OFF</span>
                    </div>
                    <div class="info-item">
                        <span class="info-label">Uptime:</span>
                        <span class="info-value" id="uptime">0s</span>
                    </div>
                    <div class="info-item">
                        <span class="info-label">Switches:</span>
                        <span class="info-value" id="switch-count">0</span>
                    </div>
                </div>
                
                <div class="controls-grid">
                    <button class="btn btn-success ripple" onclick="controlSource(1)">
                        <i class="fas fa-plug"></i> Source 1
                    </button>
                    <button class="btn btn-success ripple" onclick="controlSource(2)">
                        <i class="fas fa-plug"></i> Source 2
                    </button>
                    <button class="btn btn-danger ripple" onclick="controlSource(0)">
                        <i class="fas fa-power-off"></i> OFF
                    </button>
                    <button class="btn btn-warning ripple" onclick="toggleMode()">
                        <i class="fas fa-sync-alt"></i> Toggle Mode
                    </button>
                </div>
            </div>
        </div>
        
        <!-- Charts Section -->
        <div class="card glass">
            <div class="card-header">
                <i class="fas fa-chart-area card-icon"></i>
                <div class="card-title">Real-time Monitoring</div>
            </div>
            <div class="chart-container">
                <canvas id="voltageChart"></canvas>
            </div>
        </div>
        
        <!-- Action Buttons -->
        <div class="action-buttons">
            <button class="btn btn-primary ripple" onclick="window.open('/advanced', '_blank')">
                <i class="fas fa-tachometer-alt"></i> Advanced Dashboard
            </button>
            <button class="btn btn-primary ripple" onclick="window.open('https://%GITHUB_USERNAME%.github.io/%GITHUB_REPO%', '_blank')">
                <i class="fab fa-github"></i> GitHub Dashboard
            </button>
            <button class="btn btn-info ripple" onclick="downloadLogs()">
                <i class="fas fa-download"></i> Download Logs
            </button>
            <button class="btn btn-warning ripple" onclick="runOptimization()">
                <i class="fas fa-magic"></i> Run Optimization
            </button>
            <button class="btn btn-danger ripple" onclick="emergencyStop()">
                <i class="fas fa-exclamation-triangle"></i> Emergency Stop
            </button>
        </div>
        
        <!-- Footer -->
        <div class="footer">
            <p>Power Switch System V3 | Web Interface v3.0</p>
            <p>© 2024 Developed by mh_alliwa | Real-time Monitoring System</p>
            <p>Connection: <span id="connection-status">Checking...</span> | Last Update: <span id="last-update">--:--:--</span></p>
        </div>
    </div>
    
    <!-- Notification Container -->
    <div id="notification-container"></div>
    
    <script>
        // Global variables
        let voltageChart;
        let chartData = {
            labels: [],
            datasets: [
                { 
                    label: 'Source 1 (V)', 
                    data: [], 
                    borderColor: '#4cc9f0',
                    backgroundColor: 'rgba(76, 201, 240, 0.1)',
                    borderWidth: 3,
                    fill: true,
                    tension: 0.4,
                    pointRadius: 0
                },
                { 
                    label: 'Source 2 (V)', 
                    data: [], 
                    borderColor: '#f72585',
                    backgroundColor: 'rgba(247, 37, 133, 0.1)',
                    borderWidth: 3,
                    fill: true,
                    tension: 0.4,
                    pointRadius: 0
                }
            ]
        };
        
        // Initialize chart
        function initChart() {
            const ctx = document.getElementById('voltageChart').getContext('2d');
            voltageChart = new Chart(ctx, {
                type: 'line',
                data: chartData,
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    animation: false,
                    plugins: {
                        legend: {
                            labels: {
                                color: '#ffffff',
                                font: {
                                    size: 14
                                }
                            }
                        },
                        tooltip: {
                            mode: 'index',
                            intersect: false,
                            backgroundColor: 'rgba(26, 26, 46, 0.9)',
                            titleColor: '#4cc9f0',
                            bodyColor: '#ffffff',
                            borderColor: '#4cc9f0',
                            borderWidth: 1
                        }
                    },
                    scales: {
                        x: {
                            grid: {
                                color: 'rgba(255, 255, 255, 0.1)'
                            },
                            ticks: {
                                color: '#ffffff'
                            }
                        },
                        y: {
                            beginAtZero: false,
                            grid: {
                                color: 'rgba(255, 255, 255, 0.1)'
                            },
                            ticks: {
                                color: '#ffffff'
                            },
                            title: {
                                display: true,
                                text: 'Voltage (V)',
                                color: '#ffffff'
                            }
                        }
                    },
                    interaction: {
                        intersect: false,
                        mode: 'nearest'
                    }
                }
            });
        }
        
        // Show notification
        function showNotification(message, type = 'info', duration = 3000) {
            const container = document.getElementById('notification-container');
            const notification = document.createElement('div');
            notification.className = `notification ${type}`;
            notification.textContent = message;
            container.appendChild(notification);
            
            setTimeout(() => notification.classList.add('show'), 10);
            
            setTimeout(() => {
                notification.classList.remove('show');
                setTimeout(() => notification.remove(), 300);
            }, duration);
        }
        
        // Format time
        function formatTime(seconds) {
            const days = Math.floor(seconds / 86400);
            const hours = Math.floor((seconds % 86400) / 3600);
            const minutes = Math.floor((seconds % 3600) / 60);
            const secs = seconds % 60;
            
            if (days > 0) return `${days}d ${hours}h ${minutes}m`;
            if (hours > 0) return `${hours}h ${minutes}m ${secs}s`;
            if (minutes > 0) return `${minutes}m ${secs}s`;
            return `${secs}s`;
        }
        
        // Fetch data from API
        async function fetchData() {
            try {
                const response = await fetch('/api/status');
                const data = await response.json();
                
                // Update values
                document.getElementById('voltage1').textContent = data.voltage1.toFixed(1) + ' V';
                document.getElementById('voltage2').textContent = data.voltage2.toFixed(1) + ' V';
                document.getElementById('current').textContent = data.current.toFixed(3) + ' A';
                document.getElementById('power').textContent = data.power.toFixed(1) + ' W';
                document.getElementById('energy').textContent = data.totalEnergy.toFixed(3) + ' kWh';
                document.getElementById('switch-count').textContent = data.switchCount;
                
                // Update system status
                document.getElementById('system-status').textContent = data.systemState;
                document.getElementById('system-status').className = 'status-badge ' + 
                    (data.systemState.includes('ACTIVE') ? 'status-online' : 
                     data.systemState.includes('OFF') ? 'status-offline' : 'status-warning');
                
                document.getElementById('mode-status').textContent = data.autoMode ? 'AUTO' : 'MANUAL';
                document.getElementById('maintenance-status').textContent = data.maintenanceMode ? 'ON' : 'OFF';
                document.getElementById('uptime').textContent = formatTime(data.uptime);
                
                // Update connection status
                const connStatus = document.getElementById('connection-status');
                if (navigator.onLine) {
                    connStatus.textContent = 'Connected';
                    connStatus.style.color = '#4cc9f0';
                } else {
                    connStatus.textContent = 'Disconnected';
                    connStatus.style.color = '#f72585';
                }
                
                document.getElementById('last-update').textContent = new Date().toLocaleTimeString();
                
                // Update voltage quality
                const voltageQuality = document.getElementById('voltage-quality');
                voltageQuality.innerHTML = '';
                
                if (data.voltage1 >= 210 && data.voltage1 <= 230 && 
                    data.voltage2 >= 210 && data.voltage2 <= 230) {
                    voltageQuality.innerHTML = '<i class="fas fa-check-circle"></i> Quality: EXCELLENT';
                    voltageQuality.className = 'quality-indicator quality-excellent';
                } else if (data.voltage1 < 180 || data.voltage2 < 180) {
                    voltageQuality.innerHTML = '<i class="fas fa-exclamation-triangle"></i> Quality: LOW VOLTAGE';
                    voltageQuality.className = 'quality-indicator quality-danger';
                } else if (data.voltage1 > 250 || data.voltage2 > 250) {
                    voltageQuality.innerHTML = '<i class="fas fa-exclamation-triangle"></i> Quality: OVER VOLTAGE';
                    voltageQuality.className = 'quality-indicator quality-danger';
                } else {
                    voltageQuality.innerHTML = '<i class="fas fa-info-circle"></i> Quality: STABLE';
                    voltageQuality.className = 'quality-indicator quality-good';
                }
                
                // Update chart
                const time = new Date().toLocaleTimeString([], {hour: '2-digit', minute:'2-digit', second:'2-digit'});
                chartData.labels.push(time);
                chartData.datasets[0].data.push(data.voltage1);
                chartData.datasets[1].data.push(data.voltage2);
                
                if (chartData.labels.length > 30) {
                    chartData.labels.shift();
                    chartData.datasets[0].data.shift();
                    chartData.datasets[1].data.shift();
                }
                
                voltageChart.update('none');
                
            } catch (error) {
                console.error('Error fetching data:', error);
                showNotification('Failed to fetch data from system', 'error');
                document.getElementById('connection-status').textContent = 'Error';
                document.getElementById('connection-status').style.color = '#f72585';
            }
        }
        
        // Control power source
        async function controlSource(source) {
            let action, confirmMsg;
            
            switch(source) {
                case 0:
                    action = 'off';
                    confirmMsg = 'Are you sure you want to turn OFF the system?';
                    break;
                case 1:
                    action = 'source1';
                    confirmMsg = 'Switch to Source 1?';
                    break;
                case 2:
                    action = 'source2';
                    confirmMsg = 'Switch to Source 2?';
                    break;
            }
            
            if (!confirm(confirmMsg)) return;
            
            try {
                const response = await fetch('/api/control', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ action: action })
                });
                
                const result = await response.json();
                if (result.success) {
                    showNotification(result.message, 'success');
                    fetchData();
                } else {
                    showNotification('Operation failed: ' + result.message, 'error');
                }
            } catch (error) {
                showNotification('Control operation failed', 'error');
            }
        }
        
        // Toggle mode
        async function toggleMode() {
            try {
                const response = await fetch('/api/mode', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ mode: 'toggle' })
                });
                
                const result = await response.json();
                if (result.success) {
                    showNotification('Mode changed to ' + (result.autoMode ? 'AUTO' : 'MANUAL'), 'success');
                    fetchData();
                }
            } catch (error) {
                showNotification('Mode change failed', 'error');
            }
        }
        
        // Download logs
        async function downloadLogs() {
            try {
                const response = await fetch('/api/logs');
                const data = await response.json();
                
                const blob = new Blob([JSON.stringify(data, null, 2)], {type: 'application/json'});
                const url = window.URL.createObjectURL(blob);
                const a = document.createElement('a');
                a.href = url;
                a.download = `power_system_logs_${new Date().toISOString().split('T')[0]}.json`;
                document.body.appendChild(a);
                a.click();
                document.body.removeChild(a);
                window.URL.revokeObjectURL(url);
                
                showNotification('Logs downloaded successfully', 'success');
            } catch (error) {
                showNotification('Failed to download logs', 'error');
            }
        }
        
        // Run optimization
        async function runOptimization() {
            if (!confirm('Run system optimization? This will take about 30 seconds.')) return;
            
            showNotification('Starting system optimization...', 'warning');
            
            try {
                const response = await fetch('/api/optimize', { method: 'POST' });
                const result = await response.json();
                
                if (result.success) {
                    showNotification('Optimization started successfully', 'success');
                }
            } catch (error) {
                showNotification('Failed to start optimization', 'error');
            }
        }
        
        // Emergency stop
        async function emergencyStop() {
            if (!confirm('⚠️ EMERGENCY STOP ⚠️\n\nThis will immediately turn OFF the system. Continue?')) return;
            
            try {
                const response = await fetch('/api/control', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ action: 'emergency' })
                });
                
                const result = await response.json();
                if (result.success) {
                    showNotification('EMERGENCY STOP activated!', 'error');
                    fetchData();
                }
            } catch (error) {
                showNotification('Emergency stop failed', 'error');
            }
        }
        
        // Initialize on load
        window.addEventListener('DOMContentLoaded', () => {
            initChart();
            fetchData();
            
            // Replace GitHub placeholders
            const githubUser = '%GITHUB_USERNAME%';
            const githubRepo = '%GITHUB_REPO%';
            const html = document.body.innerHTML;
            document.body.innerHTML = html.replace(/\%GITHUB_USERNAME\%/g, githubUser)
                                         .replace(/\%GITHUB_REPO\%/g, githubRepo);
            
            // Update data every second
            setInterval(fetchData, 1000);
            
            // Add ripple effect to buttons
            document.querySelectorAll('.btn').forEach(btn => {
                btn.addEventListener('click', function(e) {
                    const ripple = document.createElement('span');
                    const rect = this.getBoundingClientRect();
                    const size = Math.max(rect.width, rect.height);
                    const x = e.clientX - rect.left - size / 2;
                    const y = e.clientY - rect.top - size / 2;
                    
                    ripple.style.cssText = `
                        position: absolute;
                        border-radius: 50%;
                        background: rgba(255, 255, 255, 0.7);
                        transform: scale(0);
                        animation: ripple-animation 0.6s linear;
                        width: ${size}px;
                        height: ${size}px;
                        top: ${y}px;
                        left: ${x}px;
                    `;
                    
                    this.appendChild(ripple);
                    
                    setTimeout(() => ripple.remove(), 600);
                });
            });
            
            // Add CSS for ripple animation
            const style = document.createElement('style');
            style.textContent = `
                @keyframes ripple-animation {
                    to {
                        transform: scale(4);
                        opacity: 0;
                    }
                }
            `;
            document.head.appendChild(style);
        });
        
        // Handle offline/online status
        window.addEventListener('online', () => {
            showNotification('Back online', 'success');
            fetchData();
        });
        
        window.addEventListener('offline', () => {
            showNotification('Connection lost', 'error');
        });
    </script>
</body>
</html>
)rawliteral";

const char* githubWebPage = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>⚡ Power Switch V3 - GitHub Dashboard</title>
    
    <!-- Libraries -->
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/animate.css/4.1.1/animate.min.css">
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/luxon@3.3.0"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-luxon@1.2.0"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@2.0.0"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-annotation@2.1.0"></script>
    
    <style>
        :root {
            --primary: #4361ee;
            --primary-dark: #3a0ca3;
            --primary-light: #4cc9f0;
            --secondary: #7209b7;
            --success: #2ecc71;
            --danger: #e74c3c;
            --warning: #f39c12;
            --info: #3498db;
            --dark: #1a1a2e;
            --darker: #16213e;
            --light: #f8f9fa;
            --gradient: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            --glass: rgba(255, 255, 255, 0.1);
            --glass-border: rgba(255, 255, 255, 0.2);
            --shadow: 0 8px 32px rgba(0, 0, 0, 0.2);
        }
        
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Segoe UI', system-ui, -apple-system, sans-serif;
        }
        
        body {
            background: linear-gradient(135deg, var(--darker), #0f3460);
            color: var(--light);
            min-height: 100vh;
            overflow-x: hidden;
        }
        
        /* Background Animation */
        .bg-animation {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            z-index: -2;
            overflow: hidden;
        }
        
        .bg-particle {
            position: absolute;
            background: radial-gradient(circle, var(--primary-light) 1%, transparent 1%);
            background-size: 50px 50px;
            width: 100%;
            height: 100%;
            opacity: 0.1;
            animation: particleMove 20s linear infinite;
        }
        
        @keyframes particleMove {
            0% { transform: translateY(0) translateX(0); }
            100% { transform: translateY(-100px) translateX(100px); }
        }
        
        .bg-grid {
            position: absolute;
            width: 100%;
            height: 100%;
            background-image: 
                linear-gradient(var(--glass-border) 1px, transparent 1px),
                linear-gradient(90deg, var(--glass-border) 1px, transparent 1px);
            background-size: 50px 50px;
            opacity: 0.05;
        }
        
        /* Glass Morphism */
        .glass {
            background: var(--glass);
            backdrop-filter: blur(20px);
            -webkit-backdrop-filter: blur(20px);
            border: 1px solid var(--glass-border);
            box-shadow: var(--shadow);
            border-radius: 20px;
        }
        
        .glass-dark {
            background: rgba(26, 26, 46, 0.8);
            backdrop-filter: blur(20px);
            border: 1px solid rgba(67, 97, 238, 0.3);
        }
        
        /* Header */
        .header {
            padding: 30px 40px;
            position: relative;
            overflow: hidden;
        }
        
        .header::before {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: linear-gradient(90deg, 
                transparent, 
                rgba(67, 97, 238, 0.1),
                transparent);
            animation: shine 3s infinite;
        }
        
        @keyframes shine {
            0% { transform: translateX(-100%); }
            100% { transform: translateX(100%); }
        }
        
        .logo-container {
            display: flex;
            align-items: center;
            gap: 20px;
            margin-bottom: 20px;
        }
        
        .logo-icon {
            font-size: 4rem;
            color: var(--primary-light);
            filter: drop-shadow(0 0 20px rgba(76, 201, 240, 0.5));
            animation: pulseGlow 2s infinite alternate;
        }
        
        @keyframes pulseGlow {
            0% { 
                transform: scale(1);
                filter: drop-shadow(0 0 20px rgba(76, 201, 240, 0.5));
            }
            100% { 
                transform: scale(1.1);
                filter: drop-shadow(0 0 30px rgba(76, 201, 240, 0.8));
            }
        }
        
        .logo-text {
            font-size: 3.5rem;
            font-weight: 900;
            background: linear-gradient(45deg, var(--primary-light), var(--primary));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-shadow: 0 2px 10px rgba(0, 0, 0, 0.3);
        }
        
        .logo-subtitle {
            font-size: 1.2rem;
            opacity: 0.8;
            margin-left: 10px;
        }
        
        /* Status Bar */
        .status-bar {
            display: flex;
            gap: 20px;
            flex-wrap: wrap;
            margin-top: 20px;
        }
        
        .status-badge {
            padding: 12px 24px;
            border-radius: 50px;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 10px;
            transition: all 0.3s;
            position: relative;
            overflow: hidden;
        }
        
        .status-badge::before {
            content: '';
            position: absolute;
            top: 0;
            left: -100%;
            width: 100%;
            height: 100%;
            background: linear-gradient(90deg, 
                transparent, 
                rgba(255, 255, 255, 0.2), 
                transparent);
            transition: 0.5s;
        }
        
        .status-badge:hover::before {
            left: 100%;
        }
        
        .status-online {
            background: linear-gradient(135deg, var(--success), #27ae60);
            box-shadow: 0 4px 20px rgba(46, 204, 113, 0.3);
        }
        
        .status-offline {
            background: linear-gradient(135deg, var(--danger), #c0392b);
            box-shadow: 0 4px 20px rgba(231, 76, 60, 0.3);
        }
        
        .status-warning {
            background: linear-gradient(135deg, var(--warning), #e67e22);
            box-shadow: 0 4px 20px rgba(243, 156, 18, 0.3);
        }
        
        .status-pulse {
            animation: statusPulse 2s infinite;
        }
        
        @keyframes statusPulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.7; }
        }
        
        /* Main Container */
        .container {
            max-width: 1800px;
            margin: 0 auto;
            padding: 0 20px;
        }
        
        /* Dashboard Grid */
        .dashboard-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(400px, 1fr));
            gap: 25px;
            margin-bottom: 30px;
        }
        
        /* Cards */
        .card {
            padding: 30px;
            transition: all 0.4s cubic-bezier(0.175, 0.885, 0.32, 1.275);
            position: relative;
            overflow: hidden;
        }
        
        .card:hover {
            transform: translateY(-10px) scale(1.02);
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.3);
        }
        
        .card::after {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 4px;
            background: linear-gradient(90deg, var(--primary), var(--primary-light));
            transform: scaleX(0);
            transform-origin: left;
            transition: transform 0.4s ease;
        }
        
        .card:hover::after {
            transform: scaleX(1);
        }
        
        .card-header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 25px;
            padding-bottom: 15px;
            border-bottom: 2px solid var(--glass-border);
        }
        
        .card-title {
            font-size: 1.5rem;
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .card-icon {
            font-size: 1.8rem;
            color: var(--primary-light);
        }
        
        /* Value Displays */
        .value-display {
            text-align: center;
            margin: 20px 0;
            position: relative;
        }
        
        .main-value {
            font-size: 4rem;
            font-weight: 800;
            background: linear-gradient(45deg, var(--light), var(--primary-light));
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-shadow: 0 2px 20px rgba(76, 201, 240, 0.3);
            line-height: 1;
        }
        
        .value-unit {
            font-size: 1.2rem;
            opacity: 0.8;
            margin-left: 5px;
        }
        
        .value-label {
            font-size: 1rem;
            opacity: 0.7;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-top: 10px;
        }
        
        .value-change {
            display: inline-block;
            padding: 4px 12px;
            border-radius: 20px;
            font-size: 0.9rem;
            font-weight: 600;
            margin-top: 10px;
        }
        
        .change-up {
            background: rgba(46, 204, 113, 0.2);
            color: var(--success);
        }
        
        .change-down {
            background: rgba(231, 76, 60, 0.2);
            color: var(--danger);
        }
        
        /* Mini Cards Grid */
        .mini-grid {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 15px;
            margin-top: 20px;
        }
        
        .mini-card {
            padding: 20px;
            border-radius: 15px;
            background: rgba(255, 255, 255, 0.05);
            transition: all 0.3s;
        }
        
        .mini-card:hover {
            background: rgba(255, 255, 255, 0.1);
            transform: translateY(-5px);
        }
        
        .mini-value {
            font-size: 1.8rem;
            font-weight: 700;
            margin: 5px 0;
        }
        
        /* Chart Containers */
        .chart-container {
            height: 400px;
            position: relative;
            margin: 20px 0;
        }
        
        .chart-container-lg {
            height: 500px;
        }
        
        /* Control Panel */
        .control-panel {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
            margin-top: 25px;
        }
        
        .control-btn {
            padding: 20px;
            border: none;
            border-radius: 15px;
            font-size: 1.1rem;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            display: flex;
            flex-direction: column;
            align-items: center;
            gap: 10px;
            position: relative;
            overflow: hidden;
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            color: white;
        }
        
        .control-btn i {
            font-size: 2rem;
        }
        
        .control-btn:hover {
            transform: translateY(-5px) scale(1.05);
            box-shadow: 0 10px 25px rgba(67, 97, 238, 0.4);
        }
        
        .control-btn:active {
            transform: translateY(-2px) scale(1.02);
        }
        
        .btn-success {
            background: linear-gradient(135deg, var(--success), #27ae60);
        }
        
        .btn-danger {
            background: linear-gradient(135deg, var(--danger), #c0392b);
        }
        
        .btn-warning {
            background: linear-gradient(135deg, var(--warning), #e67e22);
        }
        
        /* Tabs */
        .tabs {
            display: flex;
            gap: 10px;
            margin-bottom: 30px;
            overflow-x: auto;
            padding-bottom: 10px;
        }
        
        .tab {
            padding: 15px 30px;
            border-radius: 15px;
            background: rgba(255, 255, 255, 0.05);
            cursor: pointer;
            transition: all 0.3s;
            white-space: nowrap;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .tab:hover {
            background: rgba(255, 255, 255, 0.1);
            transform: translateY(-3px);
        }
        
        .tab.active {
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            box-shadow: 0 5px 15px rgba(67, 97, 238, 0.3);
        }
        
        .tab-badge {
            padding: 3px 8px;
            border-radius: 10px;
            font-size: 0.8rem;
            background: rgba(255, 255, 255, 0.2);
        }
        
        /* Modal */
        .modal {
            position: fixed;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(0, 0, 0, 0.8);
            backdrop-filter: blur(10px);
            display: none;
            justify-content: center;
            align-items: center;
            z-index: 1000;
            animation: fadeIn 0.3s;
        }
        
        @keyframes fadeIn {
            from { opacity: 0; }
            to { opacity: 1; }
        }
        
        .modal-content {
            width: 90%;
            max-width: 800px;
            max-height: 90vh;
            overflow-y: auto;
            animation: slideUp 0.4s;
        }
        
        @keyframes slideUp {
            from { transform: translateY(50px); opacity: 0; }
            to { transform: translateY(0); opacity: 1; }
        }
        
        /* Table */
        .data-table {
            width: 100%;
            border-collapse: collapse;
            margin-top: 20px;
        }
        
        .data-table th {
            text-align: left;
            padding: 15px;
            background: rgba(255, 255, 255, 0.05);
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            font-size: 0.9rem;
        }
        
        .data-table td {
            padding: 15px;
            border-bottom: 1px solid var(--glass-border);
        }
        
        .data-table tr:hover {
            background: rgba(255, 255, 255, 0.05);
        }
        
        /* Progress Bars */
        .progress-bar {
            height: 10px;
            background: rgba(255, 255, 255, 0.1);
            border-radius: 5px;
            overflow: hidden;
            margin: 15px 0;
        }
        
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, var(--primary-light), var(--primary));
            border-radius: 5px;
            transition: width 1s ease;
            position: relative;
        }
        
        .progress-fill::after {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: linear-gradient(90deg, 
                transparent, 
                rgba(255, 255, 255, 0.3), 
                transparent);
            animation: shimmer 2s infinite;
        }
        
        @keyframes shimmer {
            0% { transform: translateX(-100%); }
            100% { transform: translateX(100%); }
        }
        
        /* Notification System */
        .notification-center {
            position: fixed;
            top: 20px;
            right: 20px;
            z-index: 1000;
            max-width: 400px;
        }
        
        .notification {
            padding: 20px;
            border-radius: 15px;
            margin-bottom: 15px;
            display: flex;
            align-items: center;
            gap: 15px;
            animation: slideInRight 0.3s, fadeOut 0.3s 4.7s;
            position: relative;
            overflow: hidden;
        }
        
        @keyframes slideInRight {
            from { transform: translateX(100%); opacity: 0; }
            to { transform: translateX(0); opacity: 1; }
        }
        
        @keyframes fadeOut {
            to { opacity: 0; transform: translateX(100%); }
        }
        
        .notification::before {
            content: '';
            position: absolute;
            bottom: 0;
            left: 0;
            width: 100%;
            height: 4px;
            background: rgba(255, 255, 255, 0.2);
        }
        
        .notification::after {
            content: '';
            position: absolute;
            bottom: 0;
            left: 0;
            height: 4px;
            background: currentColor;
            animation: notificationTimer 5s linear;
        }
        
        @keyframes notificationTimer {
            from { width: 100%; }
            to { width: 0%; }
        }
        
        .notification-success {
            background: rgba(46, 204, 113, 0.9);
            border-left: 4px solid var(--success);
        }
        
        .notification-error {
            background: rgba(231, 76, 60, 0.9);
            border-left: 4px solid var(--danger);
        }
        
        .notification-warning {
            background: rgba(243, 156, 18, 0.9);
            border-left: 4px solid var(--warning);
        }
        
        .notification-info {
            background: rgba(52, 152, 219, 0.9);
            border-left: 4px solid var(--info);
        }
        
        /* Gauge */
        .gauge-container {
            width: 200px;
            height: 200px;
            position: relative;
            margin: 0 auto;
        }
        
        /* Floating Action Button */
        .fab {
            position: fixed;
            bottom: 30px;
            right: 30px;
            width: 60px;
            height: 60px;
            border-radius: 50%;
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            display: flex;
            justify-content: center;
            align-items: center;
            color: white;
            font-size: 1.5rem;
            cursor: pointer;
            box-shadow: 0 5px 20px rgba(67, 97, 238, 0.4);
            z-index: 100;
            transition: all 0.3s;
            animation: float 3s ease-in-out infinite;
        }
        
        @keyframes float {
            0%, 100% { transform: translateY(0); }
            50% { transform: translateY(-10px); }
        }
        
        .fab:hover {
            transform: scale(1.1) rotate(90deg);
            box-shadow: 0 10px 30px rgba(67, 97, 238, 0.6);
        }
        
        /* Loader */
        .loader {
            display: inline-block;
            width: 50px;
            height: 50px;
            border: 5px solid rgba(255, 255, 255, 0.1);
            border-radius: 50%;
            border-top-color: var(--primary-light);
            animation: spin 1s ease-in-out infinite;
        }
        
        @keyframes spin {
            to { transform: rotate(360deg); }
        }
        
        /* Responsive Design */
        @media (max-width: 1200px) {
            .dashboard-grid {
                grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
            }
            
            .main-value {
                font-size: 3rem;
            }
        }
        
        @media (max-width: 768px) {
            .header {
                padding: 20px;
            }
            
            .logo-text {
                font-size: 2.5rem;
            }
            
            .logo-icon {
                font-size: 3rem;
            }
            
            .dashboard-grid {
                grid-template-columns: 1fr;
            }
            
            .card {
                padding: 20px;
            }
            
            .main-value {
                font-size: 2.5rem;
            }
            
            .control-panel {
                grid-template-columns: repeat(2, 1fr);
            }
        }
        
        @media (max-width: 480px) {
            .control-panel {
                grid-template-columns: 1fr;
            }
            
            .tabs {
                flex-direction: column;
            }
            
            .tab {
                justify-content: center;
            }
        }
        
        /* Custom Scrollbar */
        ::-webkit-scrollbar {
            width: 10px;
        }
        
        ::-webkit-scrollbar-track {
            background: rgba(255, 255, 255, 0.05);
            border-radius: 5px;
        }
        
        ::-webkit-scrollbar-thumb {
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            border-radius: 5px;
        }
        
        ::-webkit-scrollbar-thumb:hover {
            background: linear-gradient(135deg, var(--primary-light), var(--primary));
        }
        
        /* Typing Effect */
        .typing-effect {
            overflow: hidden;
            border-right: 3px solid var(--primary-light);
            white-space: nowrap;
            animation: typing 3.5s steps(40, end), blink-caret 0.75s step-end infinite;
        }
        
        @keyframes typing {
            from { width: 0 }
            to { width: 100% }
        }
        
        @keyframes blink-caret {
            from, to { border-color: transparent }
            50% { border-color: var(--primary-light) }
        }
        
        /* Hover Effects */
        .hover-3d {
            transition: transform 0.3s, box-shadow 0.3s;
        }
        
        .hover-3d:hover {
            transform: perspective(1000px) rotateX(5deg) rotateY(5deg) translateZ(20px);
            box-shadow: 0 20px 40px rgba(0, 0, 0, 0.4);
        }
        
        /* Glitch Text Effect */
        .glitch {
            position: relative;
            animation: glitch 5s infinite;
        }
        
        @keyframes glitch {
            0% { transform: translate(0); }
            20% { transform: translate(-2px, 2px); }
            40% { transform: translate(-2px, -2px); }
            60% { transform: translate(2px, 2px); }
            80% { transform: translate(2px, -2px); }
            100% { transform: translate(0); }
        }
        
        /* Particle Button Effect */
        .particle-btn {
            position: relative;
            overflow: hidden;
        }
        
        .particle-btn span {
            position: absolute;
            border-radius: 50%;
            background-color: rgba(255, 255, 255, 0.7);
            width: 100px;
            height: 100px;
            margin-top: -50px;
            margin-left: -50px;
            animation: particle 1s;
            opacity: 0;
        }
        
        @keyframes particle {
            0% { 
                transform: scale(0.1); 
                opacity: 1; 
            }
            100% { 
                transform: scale(2); 
                opacity: 0; 
            }
        }
        
        /* Neon Glow */
        .neon-text {
            color: #fff;
            text-shadow: 
                0 0 5px #fff,
                0 0 10px #fff,
                0 0 20px var(--primary-light),
                0 0 40px var(--primary-light),
                0 0 80px var(--primary-light);
            animation: neon 1.5s ease-in-out infinite alternate;
        }
        
        @keyframes neon {
            from {
                text-shadow: 
                    0 0 5px #fff,
                    0 0 10px #fff,
                    0 0 20px var(--primary-light),
                    0 0 40px var(--primary-light),
                    0 0 80px var(--primary-light);
            }
            to {
                text-shadow: 
                    0 0 2px #fff,
                    0 0 5px #fff,
                    0 0 10px var(--primary-light),
                    0 0 20px var(--primary-light),
                    0 0 40px var(--primary-light);
            }
        }
        
        /* Data Visualization */
        .sparkline {
            height: 40px;
            width: 100%;
            position: relative;
        }
        
        .sparkline-line {
            fill: none;
            stroke: var(--primary-light);
            stroke-width: 2;
        }
        
        .sparkline-area {
            fill: rgba(76, 201, 240, 0.1);
        }
        
        /* Weather Widget Style */
        .weather-widget {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 20px;
            padding: 20px;
            border-radius: 15px;
            background: linear-gradient(135deg, #2980b9, #2c3e50);
            position: relative;
            overflow: hidden;
        }
        
        .weather-widget::before {
            content: '';
            position: absolute;
            top: -50%;
            left: -50%;
            width: 200%;
            height: 200%;
            background: linear-gradient(45deg, 
                transparent, 
                rgba(255, 255, 255, 0.1), 
                transparent);
            transform: rotate(45deg);
            animation: weatherShine 3s linear infinite;
        }
        
        @keyframes weatherShine {
            0% { transform: translateX(-100%) rotate(45deg); }
            100% { transform: translateX(100%) rotate(45deg); }
        }
        
        /* Timeline */
        .timeline {
            position: relative;
            padding-left: 30px;
        }
        
        .timeline::before {
            content: '';
            position: absolute;
            left: 15px;
            top: 0;
            bottom: 0;
            width: 2px;
            background: linear-gradient(to bottom, 
                var(--primary), 
                var(--primary-light));
        }
        
        .timeline-item {
            position: relative;
            margin-bottom: 30px;
            padding-left: 20px;
        }
        
        .timeline-item::before {
            content: '';
            position: absolute;
            left: -8px;
            top: 5px;
            width: 16px;
            height: 16px;
            border-radius: 50%;
            background: var(--primary);
            border: 3px solid var(--primary-light);
        }
        
        /* Calendar Widget */
        .calendar {
            display: grid;
            grid-template-columns: repeat(7, 1fr);
            gap: 10px;
            padding: 20px;
        }
        
        .calendar-day {
            aspect-ratio: 1;
            display: flex;
            align-items: center;
            justify-content: center;
            border-radius: 10px;
            background: rgba(255, 255, 255, 0.05);
            cursor: pointer;
            transition: all 0.3s;
        }
        
        .calendar-day:hover {
            background: rgba(255, 255, 255, 0.1);
            transform: scale(1.1);
        }
        
        .calendar-day.today {
            background: var(--primary);
            color: white;
            box-shadow: 0 0 20px rgba(67, 97, 238, 0.5);
        }
        
        /* Energy Flow Animation */
        .energy-flow {
            position: relative;
            height: 100px;
            overflow: hidden;
        }
        
        .energy-particle {
            position: absolute;
            width: 4px;
            height: 4px;
            background: var(--primary-light);
            border-radius: 50%;
            animation: energyFlow 2s linear infinite;
        }
        
        @keyframes energyFlow {
            0% { 
                transform: translateY(100px) translateX(-20px);
                opacity: 0;
            }
            10% { opacity: 1; }
            90% { opacity: 1; }
            100% { 
                transform: translateY(-20px) translateX(100px);
                opacity: 0;
            }
        }
        
        /* Avatar */
        .avatar {
            width: 50px;
            height: 50px;
            border-radius: 50%;
            background: linear-gradient(135deg, var(--primary), var(--primary-dark));
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 1.5rem;
            font-weight: bold;
            color: white;
            position: relative;
        }
        
        .avatar::after {
            content: '';
            position: absolute;
            top: -2px;
            left: -2px;
            right: -2px;
            bottom: -2px;
            border-radius: 50%;
            background: linear-gradient(45deg, 
                var(--primary-light), 
                var(--primary), 
                var(--primary-dark);
            z-index: -1;
            animation: rotate 3s linear infinite;
        }
        
        @keyframes rotate {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
    </style>
</head>
<body>
    <!-- Background Animation -->
    <div class="bg-animation">
        <div class="bg-particle"></div>
        <div class="bg-grid"></div>
    </div>
    
    <!-- Header -->
    <header class="header">
        <div class="container">
            <div class="logo-container">
                <i class="fas fa-bolt logo-icon"></i>
                <div>
                    <h1 class="logo-text">POWER SWITCH V3</h1>
                    <div class="logo-subtitle typing-effect">Advanced Power Management Dashboard</div>
                </div>
            </div>
            
            <div class="status-bar">
                <div class="status-badge status-online status-pulse">
                    <i class="fas fa-circle"></i>
                    <span id="connection-status">CONNECTING...</span>
                </div>
                <div class="status-badge" id="system-mode">
                    <i class="fas fa-cogs"></i>
                    <span>Mode: AUTO</span>
                </div>
                <div class="status-badge" id="current-source">
                    <i class="fas fa-plug"></i>
                    <span>Source: None</span>
                </div>
                <div class="status-badge" id="uptime-display">
                    <i class="fas fa-clock"></i>
                    <span>Uptime: 0s</span>
                </div>
                <div class="status-badge" id="last-update">
                    <i class="fas fa-sync"></i>
                    <span>Last: --:--:--</span>
                </div>
            </div>
        </div>
    </header>
    
    <!-- Main Content -->
    <main class="container">
        <!-- Tabs Navigation -->
        <div class="tabs">
            <div class="tab active" onclick="showTab('dashboard')">
                <i class="fas fa-tachometer-alt"></i>
                Dashboard
                <span class="tab-badge">Live</span>
            </div>
            <div class="tab" onclick="showTab('monitoring')">
                <i class="fas fa-chart-line"></i>
                Monitoring
                <span class="tab-badge" id="data-count">0</span>
            </div>
            <div class="tab" onclick="showTab('control')">
                <i class="fas fa-gamepad"></i>
                Control Panel
                <span class="tab-badge">Active</span>
            </div>
            <div class="tab" onclick="showTab('analytics')">
                <i class="fas fa-chart-bar"></i>
                Analytics
                <span class="tab-badge">AI</span>
            </div>
            <div class="tab" onclick="showTab('energy')">
                <i class="fas fa-battery-full"></i>
                Energy
                <span class="tab-badge">Smart</span>
            </div>
            <div class="tab" onclick="showTab('logs')">
                <i class="fas fa-clipboard-list"></i>
                System Logs
                <span class="tab-badge" id="log-count">0</span>
            </div>
            <div class="tab" onclick="showTab('settings')">
                <i class="fas fa-cog"></i>
                Settings
            </div>
        </div>
        
        <!-- Dashboard Tab -->
        <div id="dashboard-tab" class="tab-content">
            <div class="dashboard-grid">
                <!-- Voltage Card -->
                <div class="card glass hover-3d">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-bolt card-icon"></i>
                            Voltage Monitoring
                        </div>
                        <div class="quality-indicator" id="voltage-quality">
                            <i class="fas fa-spinner fa-spin"></i> Checking...
                        </div>
                    </div>
                    <div class="value-display">
                        <div class="main-value" id="voltage1-display">0.0<span class="value-unit">V</span></div>
                        <div class="value-label">Source 1</div>
                        <div class="value-change change-up" id="v1-change">+0.0%</div>
                    </div>
                    <div class="value-display">
                        <div class="main-value" id="voltage2-display">0.0<span class="value-unit">V</span></div>
                        <div class="value-label">Source 2</div>
                        <div class="value-change" id="v2-change">+0.0%</div>
                    </div>
                    <div class="progress-bar">
                        <div class="progress-fill" id="voltage-progress" style="width: 50%"></div>
                    </div>
                </div>
                
                <!-- Power Card -->
                <div class="card glass hover-3d">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-charging-station card-icon"></i>
                            Power Analytics
                        </div>
                        <div class="avatar">⚡</div>
                    </div>
                    <div class="value-display">
                        <div class="main-value" id="power-display">0.0<span class="value-unit">W</span></div>
                        <div class="value-label">Current Power</div>
                        <div class="value-change" id="power-change">+0.0%</div>
                    </div>
                    <div class="mini-grid">
                        <div class="mini-card">
                            <div class="value-label">Current</div>
                            <div class="mini-value" id="current-display">0.000A</div>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">Frequency</div>
                            <div class="mini-value" id="frequency-display">50.0Hz</div>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">Power Factor</div>
                            <div class="mini-value" id="pf-display">0.95</div>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">Temperature</div>
                            <div class="mini-value" id="temp-display">25°C</div>
                        </div>
                    </div>
                </div>
                
                <!-- Energy Card -->
                <div class="card glass hover-3d">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-leaf card-icon"></i>
                            Energy Consumption
                        </div>
                        <div class="gauge-container">
                            <canvas id="energy-gauge"></canvas>
                        </div>
                    </div>
                    <div class="value-display">
                        <div class="main-value" id="energy-display">0.000<span class="value-unit">kWh</span></div>
                        <div class="value-label">Total Energy</div>
                        <div class="value-change change-up" id="energy-change">+0.0%</div>
                    </div>
                    <div class="energy-flow" id="energy-flow"></div>
                    <div class="mini-grid">
                        <div class="mini-card">
                            <div class="value-label">Today</div>
                            <div class="mini-value" id="daily-energy">0.0kWh</div>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">This Month</div>
                            <div class="mini-value" id="monthly-energy">0.0kWh</div>
                        </div>
                    </div>
                </div>
            </div>
            
            <!-- Charts Section -->
            <div class="dashboard-grid">
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-wave-square card-icon"></i>
                            Real-time Voltage Chart
                        </div>
                        <div class="control-btn" onclick="toggleChart('voltage')">
                            <i class="fas fa-expand"></i>
                        </div>
                    </div>
                    <div class="chart-container">
                        <canvas id="realTimeChart"></canvas>
                    </div>
                </div>
                
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-fire card-icon"></i>
                            Power Usage Analytics
                        </div>
                        <div class="control-btn" onclick="toggleChart('power')">
                            <i class="fas fa-expand"></i>
                        </div>
                    </div>
                    <div class="chart-container">
                        <canvas id="powerChart"></canvas>
                    </div>
                </div>
            </div>
            
            <!-- Quick Stats -->
            <div class="dashboard-grid">
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-history card-icon"></i>
                            System Statistics
                        </div>
                    </div>
                    <div class="mini-grid">
                        <div class="mini-card">
                            <div class="value-label">Total Switches</div>
                            <div class="mini-value neon-text" id="total-switches">0</div>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">Fault Count</div>
                            <div class="mini-value" id="fault-count">0</div>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">Uptime</div>
                            <div class="mini-value" id="uptime-detail">0d 0h</div>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">Reliability</div>
                            <div class="mini-value" id="reliability-score">99.9%</div>
                        </div>
                    </div>
                </div>
                
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-network-wired card-icon"></i>
                            Network Status
                        </div>
                    </div>
                    <div class="mini-grid">
                        <div class="mini-card">
                            <div class="value-label">WiFi Signal</div>
                            <div class="mini-value" id="wifi-rssi">-dBm</div>
                            <div class="progress-bar">
                                <div class="progress-fill" id="wifi-strength" style="width: 75%"></div>
                            </div>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">IP Address</div>
                            <div class="mini-value" id="ip-address">0.0.0.0</div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        
        <!-- Monitoring Tab -->
        <div id="monitoring-tab" class="tab-content" style="display: none;">
            <div class="dashboard-grid">
                <div class="card glass chart-container-lg">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-chart-area card-icon"></i>
                            Advanced Monitoring
                        </div>
                        <div class="control-btn" onclick="exportData()">
                            <i class="fas fa-download"></i> Export
                        </div>
                    </div>
                    <div class="chart-container">
                        <canvas id="advancedChart"></canvas>
                    </div>
                </div>
                
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-exclamation-triangle card-icon"></i>
                            Alert History
                        </div>
                    </div>
                    <div class="chart-container" style="height: 300px;">
                        <canvas id="alertChart"></canvas>
                    </div>
                </div>
            </div>
        </div>
        
        <!-- Control Panel Tab -->
        <div id="control-tab" class="tab-content" style="display: none;">
            <div class="dashboard-grid">
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-sliders-h card-icon"></i>
                            Manual Control
                        </div>
                    </div>
                    <div class="control-panel">
                        <button class="control-btn btn-success particle-btn" onclick="controlSource(1)">
                            <i class="fas fa-power-off"></i>
                            Source 1 ON
                            <span>Switch to primary source</span>
                        </button>
                        <button class="control-btn btn-success particle-btn" onclick="controlSource(2)">
                            <i class="fas fa-power-off"></i>
                            Source 2 ON
                            <span>Switch to backup source</span>
                        </button>
                        <button class="control-btn btn-danger particle-btn" onclick="controlSource(0)">
                            <i class="fas fa-stop-circle"></i>
                            Emergency OFF
                            <span>Immediate shutdown</span>
                        </button>
                        <button class="control-btn btn-warning particle-btn" onclick="toggleAutoMode()">
                            <i class="fas fa-robot"></i>
                            Toggle Auto Mode
                            <span>Switch auto/manual</span>
                        </button>
                        <button class="control-btn particle-btn" onclick="optimizeSystem()">
                            <i class="fas fa-magic"></i>
                            Optimize System
                            <span>Auto-calibration</span>
                        </button>
                        <button class="control-btn particle-btn" onclick="restartSystem()">
                            <i class="fas fa-redo"></i>
                            Restart System
                            <span>Soft reboot</span>
                        </button>
                    </div>
                </div>
                
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-cogs card-icon"></i>
                            System Configuration
                        </div>
                    </div>
                    <div class="mini-grid">
                        <div class="mini-card">
                            <div class="value-label">Switch Threshold</div>
                            <input type="range" min="170" max="250" value="180" class="slider" id="threshold-slider">
                            <div class="mini-value" id="threshold-value">180V</div>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">Current Limit</div>
                            <input type="range" min="5" max="30" value="20" class="slider" id="current-slider">
                            <div class="mini-value" id="current-limit">20A</div>
                        </div>
                    </div>
                </div>
            </div>
        </div>
        
        <!-- Analytics Tab -->
        <div id="analytics-tab" class="tab-content" style="display: none;">
            <div class="card glass chart-container-lg">
                <div class="card-header">
                    <div class="card-title">
                        <i class="fas fa-brain card-icon"></i>
                        AI-Powered Analytics
                    </div>
                    <div class="control-btn" onclick="runPrediction()">
                        <i class="fas fa-bolt"></i> Predict
                    </div>
                </div>
                <div class="chart-container">
                    <canvas id="analyticsChart"></canvas>
                </div>
            </div>
        </div>
        
        <!-- Energy Tab -->
        <div id="energy-tab" class="tab-content" style="display: none;">
            <div class="dashboard-grid">
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-solar-panel card-icon"></i>
                            Energy Forecast
                        </div>
                    </div>
                    <div class="chart-container" style="height: 300px;">
                        <canvas id="forecastChart"></canvas>
                    </div>
                </div>
                
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-coins card-icon"></i>
                            Cost Analysis
                        </div>
                    </div>
                    <div class="value-display">
                        <div class="main-value" id="cost-today">Rp 0</div>
                        <div class="value-label">Today's Cost</div>
                    </div>
                </div>
            </div>
        </div>
        
        <!-- Logs Tab -->
        <div id="logs-tab" class="tab-content" style="display: none;">
            <div class="card glass">
                <div class="card-header">
                    <div class="card-title">
                        <i class="fas fa-terminal card-icon"></i>
                        System Logs
                    </div>
                    <div class="control-btn" onclick="clearLogs()">
                        <i class="fas fa-trash"></i> Clear
                    </div>
                </div>
                <div class="chart-container" style="height: 400px; overflow-y: auto;">
                    <table class="data-table">
                        <thead>
                            <tr>
                                <th>Time</th>
                                <th>Level</th>
                                <th>Source</th>
                                <th>Message</th>
                            </tr>
                        </thead>
                        <tbody id="log-table">
                            <tr><td colspan="4" class="text-center">Loading logs...</td></tr>
                        </tbody>
                    </table>
                </div>
            </div>
        </div>
        
        <!-- Settings Tab -->
        <div id="settings-tab" class="tab-content" style="display: none;">
            <div class="dashboard-grid">
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-wifi card-icon"></i>
                            Network Settings
                        </div>
                    </div>
                    <div class="mini-grid">
                        <div class="mini-card">
                            <div class="value-label">SSID</div>
                            <input type="text" class="settings-input" value="PowerSwitch-V3" id="wifi-ssid">
                        </div>
                        <div class="mini-card">
                            <div class="value-label">Password</div>
                            <input type="password" class="settings-input" value="********" id="wifi-password">
                        </div>
                    </div>
                    <button class="control-btn" onclick="saveSettings()">
                        <i class="fas fa-save"></i> Save Settings
                    </button>
                </div>
                
                <div class="card glass">
                    <div class="card-header">
                        <div class="card-title">
                            <i class="fas fa-bell card-icon"></i>
                            Notifications
                        </div>
                    </div>
                    <div class="mini-grid">
                        <div class="mini-card">
                            <div class="value-label">Telegram</div>
                            <label class="switch">
                                <input type="checkbox" checked>
                                <span class="slider round"></span>
                            </label>
                        </div>
                        <div class="mini-card">
                            <div class="value-label">Email</div>
                            <label class="switch">
                                <input type="checkbox">
                                <span class="slider round"></span>
                            </label>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </main>
    
    <!-- Notification Center -->
    <div class="notification-center" id="notification-center"></div>
    
    <!-- Floating Action Button -->
    <div class="fab" onclick="showQuickActions()">
        <i class="fas fa-plus"></i>
    </div>
    
    <!-- Quick Actions Modal -->
    <div class="modal" id="quick-actions-modal">
        <div class="modal-content card glass">
            <div class="card-header">
                <div class="card-title">
                    <i class="fas fa-bolt card-icon"></i>
                    Quick Actions
                </div>
                <div class="control-btn" onclick="hideModal('quick-actions-modal')">
                    <i class="fas fa-times"></i>
                </div>
            </div>
            <div class="control-panel">
                <button class="control-btn" onclick="takeScreenshot()">
                    <i class="fas fa-camera"></i> Screenshot
                </button>
                <button class="control-btn" onclick="exportReport()">
                    <i class="fas fa-file-export"></i> Export Report
                </button>
                <button class="control-btn" onclick="showHelp()">
                    <i class="fas fa-question-circle"></i> Help
                </button>
            </div>
        </div>
    </div>
    
    <script>
        // Global Variables
        let charts = {};
        let dataBuffer = [];
        let lastUpdate = Date.now();
        let connectionStatus = 'disconnected';
        let autoRefresh = true;
        let theme = 'dark';
        
        // ESP32 Configuration
        const ESP32_CONFIG = {
            endpoints: [
                'powerswitch-v3.local',
                '%ESP32_IP%',
                'esp32-power.local',
                '192.168.1.100'
            ],
            apiKey: 'power-switch-v3-2024',
            updateInterval: 1000,
            reconnectInterval: 5000
        };
        
        // Current ESP32 endpoint
        let currentEndpoint = null;
        
        // Initialize Dashboard
        async function initDashboard() {
            console.log('🚀 Initializing Power Switch V3 Dashboard...');
            
            // Discover ESP32
            await discoverESP32();
            
            // Initialize charts
            initCharts();
            
            // Start data updates
            startDataUpdates();
            
            // Setup event listeners
            setupEventListeners();
            
            // Load initial data
            loadInitialData();
            
            // Show welcome notification
            showNotification('Power Switch V3 Dashboard loaded successfully!', 'success');
            
            // Start background animations
            startBackgroundAnimations();
        }
        
        // Discover ESP32
        async function discoverESP32() {
            showNotification('🔍 Discovering ESP32...', 'info');
            
            for (const endpoint of ESP32_CONFIG.endpoints) {
                const normalizedEndpoint = endpoint.replace('%ESP32_IP%', window.location.hostname);
                
                try {
                    console.log(`Trying endpoint: ${normalizedEndpoint}`);
                    const response = await fetch(`http://${normalizedEndpoint}/api/status`, {
                        method: 'GET',
                        headers: { 'X-API-Key': ESP32_CONFIG.apiKey },
                        timeout: 2000
                    });
                    
                    if (response.ok) {
                        currentEndpoint = normalizedEndpoint;
                        showNotification(`✅ Connected to ESP32 at ${currentEndpoint}`, 'success');
                        updateConnectionStatus(true);
                        return;
                    }
                } catch (error) {
                    console.log(`Endpoint ${normalizedEndpoint} failed: ${error.message}`);
                }
            }
            
            // If no endpoint found, use mock data
            showNotification('⚠️ ESP32 not found. Using demo mode.', 'warning');
            updateConnectionStatus(false);
        }
        
        // Update Connection Status
        function updateConnectionStatus(connected) {
            const statusEl = document.getElementById('connection-status');
            const statusBadge = statusEl.parentElement;
            
            if (connected) {
                connectionStatus = 'connected';
                statusEl.textContent = 'CONNECTED';
                statusBadge.className = 'status-badge status-online status-pulse';
                statusBadge.querySelector('i').className = 'fas fa-circle';
            } else {
                connectionStatus = 'disconnected';
                statusEl.textContent = 'DEMO MODE';
                statusBadge.className = 'status-badge status-warning';
                statusBadge.querySelector('i').className = 'fas fa-exclamation-triangle';
            }
        }
        
        // Fetch Data from ESP32
        async function fetchESP32Data() {
            if (!currentEndpoint) return generateMockData();
            
            try {
                const response = await fetch(`http://${currentEndpoint}/api/status`, {
                    method: 'GET',
                    headers: { 'X-API-Key': ESP32_CONFIG.apiKey },
                    timeout: 3000
                });
                
                if (!response.ok) throw new Error('HTTP error');
                
                const data = await response.json();
                lastUpdate = Date.now();
                return data;
            } catch (error) {
                console.error('Failed to fetch from ESP32:', error);
                updateConnectionStatus(false);
                return generateMockData();
            }
        }
        
        // Generate Mock Data
        function generateMockData() {
            const baseTime = Date.now() / 1000;
            
            return {
                voltage1: 220 + Math.sin(baseTime / 10) * 10,
                voltage2: 215 + Math.cos(baseTime / 12) * 8,
                current: 5.5 + Math.sin(baseTime / 8) * 1.5,
                power: 1200 + Math.sin(baseTime / 15) * 200,
                totalEnergy: 25.5 + (baseTime / 3600) * 0.1,
                temperature: 35 + Math.sin(baseTime / 20) * 5,
                systemState: 'SOURCE1_ACTIVE',
                autoMode: true,
                maintenanceMode: false,
                switchCount: Math.floor(baseTime / 600),
                faultCount: 2,
                uptime: baseTime,
                wifiRSSI: -65,
                ipAddress: currentEndpoint || '192.168.1.100',
                timestamp: Math.floor(baseTime)
            };
        }
        
        // Initialize Charts
        function initCharts() {
            // Real-time Voltage Chart
            const realTimeCtx = document.getElementById('realTimeChart').getContext('2d');
            charts.realTime = new Chart(realTimeCtx, {
                type: 'line',
                data: {
                    datasets: [{
                        label: 'Source 1 Voltage',
                        data: [],
                        borderColor: '#4cc9f0',
                        backgroundColor: 'rgba(76, 201, 240, 0.1)',
                        borderWidth: 3,
                        fill: true,
                        tension: 0.4,
                        pointRadius: 0
                    }, {
                        label: 'Source 2 Voltage',
                        data: [],
                        borderColor: '#f72585',
                        backgroundColor: 'rgba(247, 37, 133, 0.1)',
                        borderWidth: 3,
                        fill: true,
                        tension: 0.4,
                        pointRadius: 0
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    animation: false,
                    interaction: {
                        intersect: false,
                        mode: 'index'
                    },
                    plugins: {
                        legend: {
                            labels: {
                                color: '#f8f9fa',
                                font: {
                                    size: 12
                                }
                            }
                        },
                        zoom: {
                            zoom: {
                                wheel: {
                                    enabled: true
                                },
                                pinch: {
                                    enabled: true
                                },
                                mode: 'xy'
                            },
                            pan: {
                                enabled: true,
                                mode: 'xy'
                            }
                        }
                    },
                    scales: {
                        x: {
                            type: 'realtime',
                            realtime: {
                                duration: 60000,
                                refresh: 1000,
                                delay: 2000
                            },
                            grid: {
                                color: 'rgba(255, 255, 255, 0.1)'
                            },
                            ticks: {
                                color: '#f8f9fa'
                            }
                        },
                        y: {
                            beginAtZero: false,
                            suggestedMin: 170,
                            suggestedMax: 250,
                            grid: {
                                color: 'rgba(255, 255, 255, 0.1)'
                            },
                            ticks: {
                                color: '#f8f9fa'
                            }
                        }
                    }
                }
            });
            
            // Initialize other charts
            initGaugeChart();
            initAdvancedChart();
        }
        
        // Initialize Gauge Chart
        function initGaugeChart() {
            const gaugeCtx = document.getElementById('energy-gauge').getContext('2d');
            charts.gauge = new Chart(gaugeCtx, {
                type: 'doughnut',
                data: {
                    datasets: [{
                        data: [75, 25],
                        backgroundColor: ['#4cc9f0', 'rgba(255, 255, 255, 0.1)'],
                        borderWidth: 0,
                        circumference: 270,
                        rotation: 225
                    }]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    cutout: '80%',
                    plugins: {
                        legend: {
                            display: false
                        },
                        tooltip: {
                            enabled: false
                        }
                    }
                },
                plugins: [{
                    id: 'gaugeText',
                    afterDraw: (chart) => {
                        const {ctx, chartArea: {width, height}} = chart;
                        ctx.save();
                        ctx.font = 'bold 30px sans-serif';
                        ctx.fillStyle = '#4cc9f0';
                        ctx.textAlign = 'center';
                        ctx.textBaseline = 'middle';
                        ctx.fillText('75%', width / 2, height / 2);
                        
                        ctx.font = '14px sans-serif';
                        ctx.fillStyle = '#f8f9fa';
                        ctx.fillText('Usage', width / 2, height / 2 + 25);
                        ctx.restore();
                    }
                }]
            });
        }
        
        // Initialize Advanced Chart
        function initAdvancedChart() {
            const advancedCtx = document.getElementById('advancedChart').getContext('2d');
            charts.advanced = new Chart(advancedCtx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [
                        {
                            label: 'Voltage Trend',
                            data: [],
                            borderColor: '#4cc9f0',
                            backgroundColor: 'rgba(76, 201, 240, 0.1)',
                            fill: true,
                            tension: 0.4
                        },
                        {
                            label: 'Power Trend',
                            data: [],
                            borderColor: '#f72585',
                            backgroundColor: 'rgba(247, 37, 133, 0.1)',
                            fill: true,
                            tension: 0.4,
                            yAxisID: 'y1'
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    interaction: {
                        intersect: false,
                        mode: 'index'
                    },
                    plugins: {
                        annotation: {
                            annotations: {
                                line1: {
                                    type: 'line',
                                    yMin: 180,
                                    yMax: 180,
                                    borderColor: '#f39c12',
                                    borderWidth: 2,
                                    borderDash: [5, 5],
                                    label: {
                                        display: true,
                                        content: 'Switch Threshold',
                                        backgroundColor: '#f39c12'
                                    }
                                }
                            }
                        }
                    },
                    scales: {
                        x: {
                            grid: {
                                color: 'rgba(255, 255, 255, 0.1)'
                            },
                            ticks: {
                                color: '#f8f9fa'
                            }
                        },
                        y: {
                            type: 'linear',
                            display: true,
                            position: 'left',
                            grid: {
                                color: 'rgba(255, 255, 255, 0.1)'
                            },
                            ticks: {
                                color: '#f8f9fa'
                            }
                        },
                        y1: {
                            type: 'linear',
                            display: true,
                            position: 'right',
                            grid: {
                                drawOnChartArea: false
                            },
                            ticks: {
                                color: '#f8f9fa'
                            }
                        }
                    }
                }
            });
        }
        
        // Start Data Updates
        function startDataUpdates() {
            // Update data every second
            setInterval(updateDashboardData, ESP32_CONFIG.updateInterval);
            
            // Update time displays
            setInterval(updateTimeDisplays, 1000);
            
            // Animate energy flow
            setInterval(animateEnergyFlow, 100);
            
            // Try to reconnect periodically
            setInterval(async () => {
                if (!currentEndpoint || Date.now() - lastUpdate > ESP32_CONFIG.reconnectInterval) {
                    await discoverESP32();
                }
            }, ESP32_CONFIG.reconnectInterval);
        }
        
        // Update Dashboard Data
        async function updateDashboardData() {
            const data = await fetchESP32Data();
            
            // Update displays
            updateDisplayValues(data);
            
            // Update charts
            updateCharts(data);
            
            // Update data buffer
            dataBuffer.push({
                timestamp: Date.now(),
                ...data
            });
            
            // Keep buffer size manageable
            if (dataBuffer.length > 1000) {
                dataBuffer = dataBuffer.slice(-500);
            }
            
            // Update counters
            document.getElementById('data-count').textContent = dataBuffer.length;
            document.getElementById('log-count').textContent = Math.floor(dataBuffer.length / 10);
            
            lastUpdate = Date.now();
        }
        
        // Update Display Values
        function updateDisplayValues(data) {
            // Voltage
            document.getElementById('voltage1-display').innerHTML = 
                `<span class="neon-text">${data.voltage1.toFixed(1)}</span><span class="value-unit">V</span>`;
            document.getElementById('voltage2-display').innerHTML = 
                `${data.voltage2.toFixed(1)}<span class="value-unit">V</span>`;
            
            // Power
            document.getElementById('power-display').innerHTML = 
                `${data.power.toFixed(1)}<span class="value-unit">W</span>`;
            document.getElementById('current-display').textContent = 
                `${data.current.toFixed(3)}A`;
            
            // Energy
            document.getElementById('energy-display').innerHTML = 
                `${data.totalEnergy.toFixed(3)}<span class="value-unit">kWh</span>`;
            
            // Temperature
            document.getElementById('temp-display').textContent = 
                `${data.temperature.toFixed(1)}°C`;
            
            // System info
            document.getElementById('system-mode').querySelector('span').textContent = 
                `Mode: ${data.autoMode ? 'AUTO' : 'MANUAL'}`;
            document.getElementById('current-source').querySelector('span').textContent = 
                `Source: ${data.systemState.includes('1') ? '1' : data.systemState.includes('2') ? '2' : 'None'}`;
            document.getElementById('total-switches').textContent = data.switchCount;
            document.getElementById('fault-count').textContent = data.faultCount;
            document.getElementById('wifi-rssi').textContent = `${data.wifiRSSI} dBm`;
            document.getElementById('ip-address').textContent = data.ipAddress;
            
            // Update progress bars
            const voltagePercent = ((data.voltage1 - 170) / 80) * 100;
            document.getElementById('voltage-progress').style.width = `${Math.min(100, Math.max(0, voltagePercent))}%`;
            
            const wifiPercent = ((data.wifiRSSI + 100) / 70) * 100;
            document.getElementById('wifi-strength').style.width = `${Math.min(100, Math.max(0, wifiPercent))}%`;
            
            // Update quality indicator
            const qualityEl = document.getElementById('voltage-quality');
            if (data.voltage1 >= 210 && data.voltage1 <= 230) {
                qualityEl.innerHTML = '<i class="fas fa-check-circle"></i> Quality: EXCELLENT';
                qualityEl.className = 'quality-indicator';
                qualityEl.style.background = 'rgba(46, 204, 113, 0.2)';
            } else if (data.voltage1 < 180) {
                qualityEl.innerHTML = '<i class="fas fa-exclamation-triangle"></i> Quality: LOW';
                qualityEl.className = 'quality-indicator';
                qualityEl.style.background = 'rgba(231, 76, 60, 0.2)';
            } else {
                qualityEl.innerHTML = '<i class="fas fa-info-circle"></i> Quality: GOOD';
                qualityEl.className = 'quality-indicator';
                qualityEl.style.background = 'rgba(52, 152, 219, 0.2)';
            }
        }
        
        // Update Charts
        function updateCharts(data) {
            const now = Date.now();
            
            // Update real-time chart
            if (charts.realTime) {
                const time = new Date(now);
                
                // Add new data point
                charts.realTime.data.datasets[0].data.push({
                    x: time,
                    y: data.voltage1
                });
                
                charts.realTime.data.datasets[1].data.push({
                    x: time,
                    y: data.voltage2
                });
                
                // Remove old data points
                const maxPoints = 60;
                if (charts.realTime.data.datasets[0].data.length > maxPoints) {
                    charts.realTime.data.datasets[0].data.shift();
                    charts.realTime.data.datasets[1].data.shift();
                }
                
                charts.realTime.update('none');
            }
            
            // Update advanced chart
            if (charts.advanced) {
                charts.advanced.data.labels.push(new Date(now).toLocaleTimeString());
                charts.advanced.data.datasets[0].data.push(data.voltage1);
                charts.advanced.data.datasets[1].data.push(data.power);
                
                if (charts.advanced.data.labels.length > 20) {
                    charts.advanced.data.labels.shift();
                    charts.advanced.data.datasets[0].data.shift();
                    charts.advanced.data.datasets[1].data.shift();
                }
                
                charts.advanced.update('none');
            }
        }
        
        // Update Time Displays
        function updateTimeDisplays() {
            const now = new Date();
            
            // Update last update time
            const secondsAgo = Math.floor((Date.now() - lastUpdate) / 1000);
            document.getElementById('last-update').querySelector('span').textContent = 
                `Last: ${secondsAgo}s ago`;
            
            // Update uptime
            const uptimeSeconds = Math.floor((Date.now() - window.startTime) / 1000);
            const days = Math.floor(uptimeSeconds / 86400);
            const hours = Math.floor((uptimeSeconds % 86400) / 3600);
            const minutes = Math.floor((uptimeSeconds % 3600) / 60);
            const seconds = uptimeSeconds % 60;
            
            document.getElementById('uptime-display').querySelector('span').textContent = 
                `Uptime: ${days}d ${hours}h ${minutes}m`;
            
            document.getElementById('uptime-detail').textContent = 
                `${days}d ${hours}h`;
        }
        
        // Animate Energy Flow
        function animateEnergyFlow() {
            const flowContainer = document.getElementById('energy-flow');
            if (!flowContainer) return;
            
            // Create new particle
            if (Math.random() < 0.3) {
                const particle = document.createElement('div');
                particle.className = 'energy-particle';
                particle.style.left = `${Math.random() * 100}%`;
                particle.style.animationDelay = `${Math.random() * 2}s`;
                flowContainer.appendChild(particle);
                
                // Remove particle after animation
                setTimeout(() => {
                    if (particle.parentNode === flowContainer) {
                        flowContainer.removeChild(particle);
                    }
                }, 2000);
            }
        }
        
        // Show Notification
        function showNotification(message, type = 'info') {
            const container = document.getElementById('notification-center');
            const notification = document.createElement('div');
            notification.className = `notification notification-${type}`;
            
            let icon = 'info-circle';
            switch(type) {
                case 'success': icon = 'check-circle'; break;
                case 'error': icon = 'exclamation-circle'; break;
                case 'warning': icon = 'exclamation-triangle'; break;
            }
            
            notification.innerHTML = `
                <i class="fas fa-${icon}"></i>
                <span>${message}</span>
            `;
            
            container.appendChild(notification);
            
            // Remove notification after 5 seconds
            setTimeout(() => {
                if (notification.parentNode === container) {
                    container.removeChild(notification);
                }
            }, 5000);
        }
        
        // Control Power Source
        async function controlSource(source) {
            const btn = event.currentTarget;
            
            // Add particle effect
            createParticleEffect(btn);
            
            let action, message;
            switch(source) {
                case 0:
                    action = 'emergency';
                    message = 'Emergency Stop';
                    break;
                case 1:
                    action = 'source1';
                    message = 'Switch to Source 1';
                    break;
                case 2:
                    action = 'source2';
                    message = 'Switch to Source 2';
                    break;
            }
            
            if (!confirm(`Are you sure you want to ${message}?`)) return;
            
            try {
                const response = await fetchESP32Control(action);
                
                if (response.success) {
                    showNotification(`${message} successful`, 'success');
                    // Refresh data
                    updateDashboardData();
                } else {
                    showNotification(`${message} failed: ${response.message}`, 'error');
                }
            } catch (error) {
                showNotification(`Control failed: ${error.message}`, 'error');
            }
        }
        
        // Send Control Command to ESP32
        async function fetchESP32Control(action) {
            if (!currentEndpoint) {
                return { success: true, message: 'Demo mode - action simulated' };
            }
            
            try {
                const response = await fetch(`http://${currentEndpoint}/api/control`, {
                    method: 'POST',
                    headers: {
                        'Content-Type': 'application/json',
                        'X-API-Key': ESP32_CONFIG.apiKey
                    },
                    body: JSON.stringify({ action }),
                    timeout: 5000
                });
                
                return await response.json();
            } catch (error) {
                throw new Error('Network error');
            }
        }
        
        // Create Particle Effect
        function createParticleEffect(element) {
            const rect = element.getBoundingClientRect();
            const particleCount = 20;
            
            for (let i = 0; i < particleCount; i++) {
                const particle = document.createElement('span');
                const size = Math.random() * 20 + 5;
                const x = Math.random() * rect.width;
                const y = Math.random() * rect.height;
                
                particle.style.cssText = `
                    position: absolute;
                    width: ${size}px;
                    height: ${size}px;
                    background: rgba(255, 255, 255, 0.7);
                    border-radius: 50%;
                    pointer-events: none;
                    left: ${x}px;
                    top: ${y}px;
                `;
                
                element.appendChild(particle);
                
                // Animate particle
                const angle = Math.random() * Math.PI * 2;
                const speed = Math.random() * 100 + 50;
                const duration = Math.random() * 0.5 + 0.5;
                
                particle.animate([
                    {
                        transform: 'translate(0, 0) scale(1)',
                        opacity: 1
                    },
                    {
                        transform: `translate(${Math.cos(angle) * speed}px, ${Math.sin(angle) * speed}px) scale(0)`,
                        opacity: 0
                    }
                ], {
                    duration: duration * 1000,
                    easing: 'cubic-bezier(0.215, 0.610, 0.355, 1)'
                });
                
                // Remove particle after animation
                setTimeout(() => {
                    if (particle.parentNode === element) {
                        element.removeChild(particle);
                    }
                }, duration * 1000);
            }
        }
        
        // Toggle Auto Mode
        async function toggleAutoMode() {
            try {
                const response = await fetchESP32Control('toggle-mode');
                
                if (response.success) {
                    showNotification(`Mode changed to ${response.autoMode ? 'AUTO' : 'MANUAL'}`, 'success');
                    updateDashboardData();
                }
            } catch (error) {
                showNotification('Mode change failed', 'error');
            }
        }
        
        // Optimize System
        async function optimizeSystem() {
            if (!confirm('Run system optimization? This will take about 30 seconds.')) return;
            
            showNotification('Starting system optimization...', 'info');
            
            try {
                const response = await fetchESP32Control('optimize');
                
                if (response.success) {
                    showNotification('✅ System optimization started!', 'success');
                }
            } catch (error) {
                showNotification('Optimization failed', 'error');
            }
        }
        
        // Restart System
        async function restartSystem() {
            if (!confirm('Are you sure you want to restart the system?')) return;
            
            showNotification('System restarting...', 'warning');
            
            try {
                const response = await fetchESP32Control('restart');
                
                if (response.success) {
                    showNotification('✅ System restart initiated!', 'success');
                    setTimeout(() => {
                        location.reload();
                    }, 3000);
                }
            } catch (error) {
                showNotification('Restart failed', 'error');
            }
        }
        
        // Show Tab
        function showTab(tabName) {
            // Hide all tabs
            document.querySelectorAll('.tab-content').forEach(tab => {
                tab.style.display = 'none';
            });
            
            // Remove active class from all tabs
            document.querySelectorAll('.tab').forEach(tab => {
                tab.classList.remove('active');
            });
            
            // Show selected tab
            document.getElementById(`${tabName}-tab`).style.display = 'block';
            
            // Add active class to clicked tab
            event.currentTarget.classList.add('active');
        }
        
        // Export Data
        function exportData() {
            const dataStr = JSON.stringify(dataBuffer, null, 2);
            const dataUri = 'data:application/json;charset=utf-8,'+ encodeURIComponent(dataStr);
            
            const exportFileDefaultName = `power-switch-data-${new Date().toISOString()}.json`;
            
            const linkElement = document.createElement('a');
            linkElement.setAttribute('href', dataUri);
            linkElement.setAttribute('download', exportFileDefaultName);
            linkElement.click();
            
            showNotification('Data exported successfully!', 'success');
        }
        
        // Setup Event Listeners
        function setupEventListeners() {
            // Slider events
            document.getElementById('threshold-slider').addEventListener('input', function() {
                document.getElementById('threshold-value').textContent = this.value + 'V';
            });
            
            document.getElementById('current-slider').addEventListener('input', function() {
                document.getElementById('current-limit').textContent = this.value + 'A';
            });
            
            // Close modals on click outside
            document.addEventListener('click', function(event) {
                const modals = document.querySelectorAll('.modal');
                modals.forEach(modal => {
                    if (event.target === modal) {
                        modal.style.display = 'none';
                    }
                });
            });
            
            // Keyboard shortcuts
            document.addEventListener('keydown', function(event) {
                // Ctrl + S to save
                if (event.ctrlKey && event.key === 's') {
                    event.preventDefault();
                    saveSettings();
                }
                
                // F5 to refresh
                if (event.key === 'F5') {
                    event.preventDefault();
                    location.reload();
                }
                
                // Escape to close modals
                if (event.key === 'Escape') {
                    document.querySelectorAll('.modal').forEach(modal => {
                        modal.style.display = 'none';
                    });
                }
            });
        }
        
        // Load Initial Data
        function loadInitialData() {
            window.startTime = Date.now();
            
            // Set initial values
            document.getElementById('daily-energy').textContent = '12.5kWh';
            document.getElementById('monthly-energy').textContent = '356.8kWh';
            document.getElementById('cost-today').textContent = 'Rp 25.000';
            document.getElementById('reliability-score').textContent = '99.9%';
            document.getElementById('frequency-display').textContent = '50.0Hz';
            document.getElementById('pf-display').textContent = '0.95';
        }
        
        // Initialize on load
        document.addEventListener('DOMContentLoaded', async () => {
            // Replace placeholders
            const html = document.body.innerHTML;
            document.body.innerHTML = html
                .replace(/\%GITHUB_USERNAME\%/g, '%GITHUB_USERNAME%')
                .replace(/\%GITHUB_REPO\%/g, '%GITHUB_REPO%')
                .replace(/\%ESP32_IP\%/g, window.location.hostname)
                .replace(/\%ESP32_HOST\%/g, 'powerswitch-v3.local');
            
            // Initialize dashboard
            await initDashboard();
        });
    </script>
</body>
</html>
)rawliteral";

// ========== SYSTEM LOG FUNCTIONS ==========
void addSystemLog(String level, String source, String message) {
  systemLogs[logIndex].timestamp = timeClient.getEpochTime();
  systemLogs[logIndex].level = level;
  systemLogs[logIndex].source = source;
  systemLogs[logIndex].message = message;
  
  logIndex = (logIndex + 1) % MAX_LOGS;
  
  Serial.printf("[%s] %s: %s\n", level.c_str(), source.c_str(), message.c_str());
}

// ========== TELEGRAM FUNCTIONS ==========
void sendTelegramAlert(String message) {
  if (notificationEnabled && WiFi.status() == WL_CONNECTED) {
    if (bot.sendMessage(CHAT_ID, message, "Markdown")) {
      addSystemLog("INFO", "Telegram", "Alert sent: " + message.substring(0, 50) + "...");
    } else {
      addSystemLog("ERROR", "Telegram", "Failed to send alert");
    }
  }
}

void handleTelegramMessages() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 1000) {
    lastCheck = millis();
    
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    
    while(numNewMessages) {
      for (int i = 0; i < numNewMessages; i++) {
        String chat_id = String(bot.messages[i].chat_id);
        if (chat_id != CHAT_ID) {
          bot.sendMessage(chat_id, "Unauthorized access!", "");
          continue;
        }
        
        String text = bot.messages[i].text;
        String from_name = bot.messages[i].from_name;
        
        if (text == "/start") {
          String welcome = "🤖 *POWER SWITCH V3 BOT*\n\n";
          welcome += "Welcome, " + from_name + "!\n";
          welcome += "System Status: " + getStateText() + "\n";
          welcome += "Mode: " + String(autoMode ? "AUTO" : "MANUAL") + "\n";
          welcome += "Energy: " + String(totalEnergy, 2) + " kWh\n\n";
          welcome += "*Commands:*\n";
          welcome += "/status - System status\n";
          welcome += "/on - Turn ON\n";
          welcome += "/off - Turn OFF\n";
          welcome += "/source1 - Switch to Source 1\n";
          welcome += "/source2 - Switch to Source 2\n";
          welcome += "/mode - Toggle auto/manual\n";
          welcome += "/optimize - Run optimization\n";
          welcome += "/logs - View recent logs\n";
          welcome += "/help - Show this message";
          
          bot.sendMessage(CHAT_ID, welcome, "Markdown");
        }
        else if (text == "/status") {
          String status = "📊 *SYSTEM STATUS*\n\n";
          status += "State: " + getStateText() + "\n";
          status += "Mode: " + String(autoMode ? "AUTO" : "MANUAL") + "\n";
          status += "V1: " + String(sensors.voltage1, 1) + " V\n";
          status += "V2: " + String(sensors.voltage2, 1) + " V\n";
          status += "Current: " + String(sensors.current, 2) + " A\n";
          status += "Power: " + String(sensors.power, 1) + " W\n";
          status += "Energy: " + String(totalEnergy, 3) + " kWh\n";
          status += "Temp: " + String(sensors.temperature, 1) + " °C\n";
          status += "Uptime: " + String(millis() / 1000) + "s\n";
          status += "IP: " + WiFi.localIP().toString();
          
          bot.sendMessage(CHAT_ID, status, "Markdown");
        }
        else if (text == "/on" && sensors.voltage1 > 100) {
          digitalWrite(RELAY1_PIN, LOW);
          digitalWrite(RELAY2_PIN, HIGH);
          systemState = STATE_SOURCE1_ACTIVE;
          bot.sendMessage(CHAT_ID, "✅ System turned ON (Source 1)", "");
        }
        else if (text == "/off") {
          digitalWrite(RELAY1_PIN, HIGH);
          digitalWrite(RELAY2_PIN, HIGH);
          systemState = STATE_OFF;
          bot.sendMessage(CHAT_ID, "🛑 System turned OFF", "");
        }
        else if (text == "/source1" && sensors.voltage1 > 100) {
          digitalWrite(RELAY1_PIN, LOW);
          digitalWrite(RELAY2_PIN, HIGH);
          systemState = STATE_SOURCE1_ACTIVE;
          stats.switchCount++;
          bot.sendMessage(CHAT_ID, "🔁 Switched to Source 1", "");
        }
        else if (text == "/source2" && sensors.voltage2 > 100) {
          digitalWrite(RELAY1_PIN, HIGH);
          digitalWrite(RELAY2_PIN, LOW);
          systemState = STATE_SOURCE2_ACTIVE;
          stats.switchCount++;
          bot.sendMessage(CHAT_ID, "🔁 Switched to Source 2", "");
        }
        else if (text == "/mode") {
          autoMode = !autoMode;
          bot.sendMessage(CHAT_ID, "🔧 Mode: " + String(autoMode ? "AUTO" : "MANUAL"), "");
        }
        else if (text == "/optimize") {
          bot.sendMessage(CHAT_ID, "🔧 Starting system optimization...", "");
          performOptimization();
        }
        else if (text == "/logs") {
          String logs = "📝 *RECENT LOGS*\n\n";
          int count = 0;
          for (int j = 0; j < min(10, MAX_LOGS); j++) {
            int idx = (logIndex - 1 - j + MAX_LOGS) % MAX_LOGS;
            if (systemLogs[idx].timestamp > 0) {
              logs += "[" + getTimeString() + "] ";
              logs += systemLogs[idx].level + ": ";
              logs += systemLogs[idx].message + "\n";
              count++;
            }
          }
          if (count == 0) logs += "No logs available";
          bot.sendMessage(CHAT_ID, logs, "");
        }
        else if (text == "/help") {
          String help = "🆘 *HELP MENU*\n\n";
          help += "Available Commands:\n";
          help += "/start - Start bot\n";
          help += "/status - System status\n";
          help += "/on - Turn system ON\n";
          help += "/off - Turn system OFF\n";
          help += "/source1 - Use Source 1\n";
          help += "/source2 - Use Source 2\n";
          help += "/mode - Toggle auto/manual\n";
          help += "/optimize - Run optimization\n";
          help += "/logs - View system logs\n";
          help += "/help - Show this message\n\n";
          help += "Developer: @mh_alliwa";
          bot.sendMessage(CHAT_ID, help, "Markdown");
        }
        else {
          bot.sendMessage(CHAT_ID, "❓ Unknown command. Type /help for commands.", "");
        }
      }
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
  }
}

// ========== EMERGENCY FUNCTION ==========
void emergencyShutdown(String reason) {
  Serial.println("🚨 EMERGENCY: " + reason);
  
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  systemState = STATE_FAULT;
  stats.faultCount++;
  
  String message = "🚨 *EMERGENCY SHUTDOWN!*\n";
  message += "Reason: " + reason + "\n";
  message += "V1: " + String(sensors.voltage1, 1) + "V\n";
  message += "V2: " + String(sensors.voltage2, 1) + "V\n";
  message += "I: " + String(sensors.current, 2) + "A\n";
  message += "Temp: " + String(sensors.temperature, 1) + "°C\n";
  message += "All relays turned OFF\n";
  message += "System requires manual reset";
  
  sendTelegramAlert(message);
  addSystemLog("CRITICAL", "Safety", reason);
  
  // Alert pattern
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_STATUS, !digitalRead(LED_STATUS));
    digitalWrite(BUZZER_PIN, !digitalRead(BUZZER_PIN));
    delay(200);
  }
  
  digitalWrite(BUZZER_PIN, LOW);
}

// ========== STATE TEXT FUNCTION ==========
String getStateText() {
  switch (systemState) {
    case STATE_OFF: return "OFF";
    case STATE_SOURCE1_ACTIVE: return "SOURCE 1 ACTIVE";
    case STATE_SOURCE2_ACTIVE: return "SOURCE 2 ACTIVE";
    case STATE_SWITCHING: return "SWITCHING";
    case STATE_FAULT: return "FAULT";
    case STATE_OPTIMIZATION: return "OPTIMIZATION";
    case STATE_UPDATE: return "UPDATING";
    default: return "UNKNOWN";
  }
}

// ========== TIME FUNCTIONS ==========
String getTimeString() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);
  
  char timeStr[9];
  sprintf(timeStr, "%02d:%02d:%02d", 
          ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  return String(timeStr);
}

String getDateString() {
  timeClient.update();
  unsigned long epochTime = timeClient.getEpochTime();
  struct tm *ptm = gmtime ((time_t *)&epochTime);
  
  char dateStr[11];
  sprintf(dateStr, "%04d-%02d-%02d", 
          ptm->tm_year + 1900, ptm->tm_mon + 1, ptm->tm_mday);
  return String(dateStr);
}

// ========== TEMPERATURE FUNCTION ==========
float readTemperature() {
  int raw = analogRead(TEMP_SENSOR_PIN);
  float voltage = raw * 3.3 / 4095.0;
  // LM35: 10mV per °C
  return voltage * 100.0;
}

// ========== SENSOR FUNCTIONS ==========
float readTrueRMS(uint8_t pin, int zeroPoint) {
  const int samples = 2000;
  unsigned long sumSquares = 0;
  
  for (int i = 0; i < samples; i++) {
    int raw = analogRead(pin);
    int offset = raw - zeroPoint;
    sumSquares += (unsigned long)offset * offset;
    delayMicroseconds(40);
  }
  
  return sqrt((float)sumSquares / samples);
}

float convertToVoltage(float rmsADC, float calibFactor, float offset = 0) {
  float voltageADC = rmsADC * 3.3 / 4095;
  float realVoltage = (voltageADC * calibFactor * 250.0) / (1.8 * 0.7071) + offset;
  return realVoltage;
}

void readSensors() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < 500) return;
  lastRead = millis();
  
  // Read RMS values
  float rmsV1 = readTrueRMS(PIN_ZMPT1, optim.zeroV1);
  float rmsV2 = readTrueRMS(PIN_ZMPT2, optim.zeroV2);
  float rmsI = readTrueRMS(PIN_ACS, optim.zeroI);
  
  // Convert to actual values
  sensors.voltage1 = convertToVoltage(rmsV1, optim.calibrationFactorV1, optim.voltageOffsetV1);
  sensors.voltage2 = convertToVoltage(rmsV2, optim.calibrationFactorV2, optim.voltageOffsetV2);
  
  // Current calculation
  float voltageAdcI = rmsI * 3.3 / 4095;
  sensors.current = (voltageAdcI - (3.3/2)) / 0.100 * optim.calibrationFactorI;
  
  // Read temperature
  sensors.temperature = readTemperature();
  
  // Store in history
  voltageHistory1[historyIndex] = sensors.voltage1;
  voltageHistory2[historyIndex] = sensors.voltage2;
  currentHistory[historyIndex] = sensors.current;
  powerHistory[historyIndex] = sensors.power;
  historyIndex = (historyIndex + 1) % DATA_BUFFER_SIZE;
  
  // Calculate power
  if (systemState == STATE_SOURCE1_ACTIVE && sensors.voltage1 > 10) {
    sensors.power = sensors.voltage1 * abs(sensors.current) * 0.8;
  } else if (systemState == STATE_SOURCE2_ACTIVE && sensors.voltage2 > 10) {
    sensors.power = sensors.voltage2 * abs(sensors.current) * 0.8;
  } else {
    sensors.power = 0;
  }
  
  // Update energy
  if (sensors.power > 0 && lastEnergyUpdate > 0) {
    float hours = (millis() - lastEnergyUpdate) / 3600000.0;
    float energyAdd = sensors.power * hours / 1000.0;
    totalEnergy += energyAdd;
    dailyEnergy += energyAdd;
    monthlyEnergy += energyAdd;
    
    // Save energy data periodically
    static unsigned long lastEnergySave = 0;
    if (millis() - lastEnergySave > 60000) {
      lastEnergySave = millis();
      preferences.begin("power-switch", false);
      preferences.putFloat("totalEnergy", totalEnergy);
      preferences.putFloat("dailyEnergy", dailyEnergy);
      preferences.putFloat("monthlyEnergy", monthlyEnergy);
      preferences.end();
    }
  }
  lastEnergyUpdate = millis();
  
  // Check safety limits
  checkSafety();
  
  // Log sensor data
  static unsigned long lastSensorLog = 0;
  if (millis() - lastSensorLog > 30000) {
    lastSensorLog = millis();
    String logMsg = "V1: " + String(sensors.voltage1, 1) + "V, ";
    logMsg += "V2: " + String(sensors.voltage2, 1) + "V, ";
    logMsg += "I: " + String(sensors.current, 2) + "A, ";
    logMsg += "P: " + String(sensors.power, 1) + "W";
    addSystemLog("INFO", "Sensors", logMsg);
  }
}

// ========== SAFETY CHECK ==========
void checkSafety() {
  // Voltage safety
  if (sensors.voltage1 > voltageHighThreshold || sensors.voltage2 > voltageHighThreshold) {
    emergencyShutdown("Over-voltage detected!");
  }
  
  // Current safety
  if (abs(sensors.current) > currentHighThreshold) {
    emergencyShutdown("Over-current detected!");
  }
  
  // Temperature safety
  if (sensors.temperature > temperatureThreshold) {
    emergencyShutdown("Over-temperature detected!");
  }
  
  // Low voltage warning
  static bool lowVoltageAlerted = false;
  if ((sensors.voltage1 < voltageLowThreshold && systemState == STATE_SOURCE1_ACTIVE) ||
      (sensors.voltage2 < voltageLowThreshold && systemState == STATE_SOURCE2_ACTIVE)) {
    if (!lowVoltageAlerted) {
      String alert = "⚠️ *LOW VOLTAGE WARNING*\n";
      alert += "Current voltage: ";
      alert += String(systemState == STATE_SOURCE1_ACTIVE ? sensors.voltage1 : sensors.voltage2, 1);
      alert += "V (Threshold: " + String(voltageLowThreshold) + "V)";
      sendTelegramAlert(alert);
      addSystemLog("WARNING", "Safety", "Low voltage detected");
      lowVoltageAlerted = true;
    }
  } else {
    lowVoltageAlerted = false;
  }
}

// ========== AUTO SWITCH FUNCTION ==========
void performAutoSwitch() {
  if (!autoMode || maintenanceMode) return;
  
  unsigned long currentTime = millis();
  
  // Prevent rapid switching
  if (currentTime - stats.lastSwitchTime < 5000) return;
  
  if (systemState == STATE_SOURCE1_ACTIVE) {
    if (sensors.voltage1 < 180 && sensors.voltage2 >= 190) {
      systemState = STATE_SWITCHING;
      addSystemLog("INFO", "Auto-Switch", "Switching to Source 2 (V1 low)");
      
      digitalWrite(RELAY1_PIN, HIGH);
      delay(100);
      digitalWrite(RELAY2_PIN, LOW);
      
      systemState = STATE_SOURCE2_ACTIVE;
      stats.switchCount++;
      stats.lastSwitchTime = currentTime;
      
      String message = "🔁 *AUTO SWITCH*\n";
      message += "Source 1: " + String(sensors.voltage1, 1) + "V (LOW)\n";
      message += "Source 2: " + String(sensors.voltage2, 1) + "V (OK)\n";
      message += "Switched to Source 2";
      sendTelegramAlert(message);
    }
  }
  else if (systemState == STATE_SOURCE2_ACTIVE) {
    if (sensors.voltage2 < 180 && sensors.voltage1 >= 190) {
      systemState = STATE_SWITCHING;
      addSystemLog("INFO", "Auto-Switch", "Switching to Source 1 (V2 low)");
      
      digitalWrite(RELAY2_PIN, HIGH);
      delay(100);
      digitalWrite(RELAY1_PIN, LOW);
      
      systemState = STATE_SOURCE1_ACTIVE;
      stats.switchCount++;
      stats.lastSwitchTime = currentTime;
      
      String message = "🔁 *AUTO SWITCH*\n";
      message += "Source 2: " + String(sensors.voltage2, 1) + "V (LOW)\n";
      message += "Source 1: " + String(sensors.voltage1, 1) + "V (OK)\n";
      message += "Switched to Source 1";
      sendTelegramAlert(message);
    }
  }
  else if (systemState == STATE_OFF) {
    // Auto-start if voltage available
    if (sensors.voltage1 >= 180) {
      digitalWrite(RELAY1_PIN, LOW);
      digitalWrite(RELAY2_PIN, HIGH);
      systemState = STATE_SOURCE1_ACTIVE;
      stats.switchCount++;
      stats.lastSwitchTime = currentTime;
      
      String message = "🔁 *AUTO START*\n";
      message += "Voltage detected: " + String(sensors.voltage1, 1) + "V\n";
      message += "System started automatically";
      sendTelegramAlert(message);
      addSystemLog("INFO", "Auto-Switch", "Auto-started on Source 1");
    }
    else if (sensors.voltage2 >= 180) {
      digitalWrite(RELAY1_PIN, HIGH);
      digitalWrite(RELAY2_PIN, LOW);
      systemState = STATE_SOURCE2_ACTIVE;
      stats.switchCount++;
      stats.lastSwitchTime = currentTime;
      
      String message = "🔁 *AUTO START*\n";
      message += "Voltage detected: " + String(sensors.voltage2, 1) + "V\n";
      message += "System started automatically on Source 2";
      sendTelegramAlert(message);
      addSystemLog("INFO", "Auto-Switch", "Auto-started on Source 2");
    }
  }
}

// ========== OPTIMIZATION FUNCTION ==========
void performOptimization() {
  Serial.println("\n🎯 STARTING SYSTEM OPTIMIZATION");
  systemState = STATE_OPTIMIZATION;
  addSystemLog("INFO", "Optimization", "Starting system optimization");
  
  // Save current state
  bool oldAutoMode = autoMode;
  autoMode = false;
  
  // Turn off all relays
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  delay(2000);
  
  // Optimize zero points
  const int OPTIM_SAMPLES = 5000;
  long sumV1 = 0, sumV2 = 0, sumI = 0;
  
  Serial.println("Calibrating zero points...");
  
  for (int i = 0; i < OPTIM_SAMPLES; i++) {
    sumV1 += analogRead(PIN_ZMPT1);
    sumV2 += analogRead(PIN_ZMPT2);
    sumI += analogRead(PIN_ACS);
    if (i % 1000 == 0) Serial.print(".");
    delayMicroseconds(50);
  }
  Serial.println();
  
  optim.zeroV1 = sumV1 / OPTIM_SAMPLES;
  optim.zeroV2 = sumV2 / OPTIM_SAMPLES;
  optim.zeroI = sumI / OPTIM_SAMPLES;
  
  Serial.printf("Zero points: V1=%d, V2=%d, I=%d\n", optim.zeroV1, optim.zeroV2, optim.zeroI);
  
  // Auto-adjust calibration factors
  float targetVoltage = 220.0;
  float measuredV1 = convertToVoltage(readTrueRMS(PIN_ZMPT1, optim.zeroV1), optim.calibrationFactorV1);
  float measuredV2 = convertToVoltage(readTrueRMS(PIN_ZMPT2, optim.zeroV2), optim.calibrationFactorV2);
  
  Serial.printf("Measured: V1=%.1fV, V2=%.1fV\n", measuredV1, measuredV2);
  
  if (measuredV1 > 50 && measuredV1 < 300) {
    optim.calibrationFactorV1 *= (targetVoltage / measuredV1);
  }
  if (measuredV2 > 50 && measuredV2 < 300) {
    optim.calibrationFactorV2 *= (targetVoltage / measuredV2);
  }
  
  // Save optimization data
  preferences.begin("power-switch", false);
  preferences.putInt("zeroV1", optim.zeroV1);
  preferences.putInt("zeroV2", optim.zeroV2);
  preferences.putInt("zeroI", optim.zeroI);
  preferences.putFloat("calV1", optim.calibrationFactorV1);
  preferences.putFloat("calV2", optim.calibrationFactorV2);
  preferences.putFloat("calI", optim.calibrationFactorI);
  preferences.end();
  
  // Restore mode
  autoMode = oldAutoMode;
  systemState = STATE_OFF;
  
  Serial.println("✅ OPTIMIZATION COMPLETE");
  Serial.printf("Calibration factors: V1=%.4f, V2=%.4f, I=%.4f\n", 
                optim.calibrationFactorV1, optim.calibrationFactorV2, optim.calibrationFactorI);
  
  addSystemLog("INFO", "Optimization", "Optimization completed successfully");
  
  // Send confirmation
  String message = "✅ *SYSTEM OPTIMIZATION COMPLETE*\n\n";
  message += "New calibration factors:\n";
  message += "• Source 1: " + String(optim.calibrationFactorV1, 4) + "\n";
  message += "• Source 2: " + String(optim.calibrationFactorV2, 4) + "\n";
  message += "• Current: " + String(optim.calibrationFactorI, 4) + "\n";
  message += "Zero points updated\n";
  message += "Accuracy improved and saved!";
  
  sendTelegramAlert(message);
}

// ========== LCD UPDATE FUNCTION ==========
void updateLCD() {
  static unsigned long lastLCDUpdate = 0;
  if (millis() - lastLCDUpdate < 1000) return;
  lastLCDUpdate = millis();
  
  static int screen = 0;
  screen = (screen + 1) % 3;
  
  lcd.clear();
  
  switch(screen) {
    case 0: // Main screen
      lcd.print(getStateText());
      lcd.setCursor(0, 1);
      lcd.printf("V1:%.0fV V2:%.0fV", sensors.voltage1, sensors.voltage2);
      break;
      
    case 1: // Power screen
      lcd.print("POWER INFO");
      lcd.setCursor(0, 1);
      lcd.printf("%.1fW %.2fA", sensors.power, sensors.current);
      break;
      
    case 2: // System info
      lcd.print("SYSTEM INFO");
      lcd.setCursor(0, 1);
      lcd.printf("E:%.1fkWh %dC", totalEnergy, (int)sensors.temperature);
      break;
  }
}

// ========== SETTINGS FUNCTIONS ==========
void saveSystemSettings() {
  preferences.begin("power-switch", false);
  preferences.putBool("autoMode", autoMode);
  preferences.putBool("maintenanceMode", maintenanceMode);
  preferences.putBool("notificationEnabled", notificationEnabled);
  preferences.putBool("dataLogging", dataLogging);
  preferences.putBool("cloudSync", cloudSync);
  preferences.putFloat("totalEnergy", totalEnergy);
  preferences.putFloat("dailyEnergy", dailyEnergy);
  preferences.putFloat("monthlyEnergy", monthlyEnergy);
  preferences.putUInt("switchCount", stats.switchCount);
  preferences.putUInt("faultCount", stats.faultCount);
  preferences.putUInt("totalUptime", stats.totalUptime + (millis() / 1000));
  preferences.end();
  
  addSystemLog("INFO", "System", "Settings saved");
}

void loadSystemSettings() {
  preferences.begin("power-switch", true);
  optim.zeroV1 = preferences.getInt("zeroV1", 2048);
  optim.zeroV2 = preferences.getInt("zeroV2", 2048);
  optim.zeroI = preferences.getInt("zeroI", 2048);
  optim.calibrationFactorV1 = preferences.getFloat("calV1", 1.0);
  optim.calibrationFactorV2 = preferences.getFloat("calV2", 1.0);
  optim.calibrationFactorI = preferences.getFloat("calI", 1.0);
  autoMode = preferences.getBool("autoMode", true);
  maintenanceMode = preferences.getBool("maintenanceMode", false);
  notificationEnabled = preferences.getBool("notificationEnabled", true);
  dataLogging = preferences.getBool("dataLogging", true);
  cloudSync = preferences.getBool("cloudSync", false);
  totalEnergy = preferences.getFloat("totalEnergy", 0);
  dailyEnergy = preferences.getFloat("dailyEnergy", 0);
  monthlyEnergy = preferences.getFloat("monthlyEnergy", 0);
  stats.switchCount = preferences.getUInt("switchCount", 0);
  stats.faultCount = preferences.getUInt("faultCount", 0);
  stats.totalUptime = preferences.getUInt("totalUptime", 0);
  stats.startupTime = preferences.getUInt("startupTime", 0);
  
  // Load alert thresholds
  voltageLowThreshold = preferences.getFloat("voltageLow", 170.0);
  voltageHighThreshold = preferences.getFloat("voltageHigh", 250.0);
  currentHighThreshold = preferences.getFloat("currentHigh", 20.0);
  temperatureThreshold = preferences.getFloat("temperatureHigh", 60.0);
  preferences.end();
  
  addSystemLog("INFO", "System", "Settings loaded");
}

// ========== WEB SERVER ROUTES ==========
void initWebServer() {
  // Setup mDNS
  if (!MDNS.begin("powerswitch-v3")) {
    Serial.println("❌ Error setting up mDNS!");
  }
  
  // Basic web interface
  server.on("/", HTTP_GET, []() {
    String page = String(basicWebPage);
    page.replace("%GITHUB_USERNAME%", githubUsername);
    page.replace("%GITHUB_REPO%", githubRepo);
    server.send(200, "text/html", page);
  });
  
  // GitHub Pages interface
  server.on("/github", HTTP_GET, []() {
    String page = String(githubWebPage);
    page.replace("%GITHUB_USERNAME%", githubUsername);
    page.replace("%GITHUB_REPO%", githubRepo);
    page.replace("%ESP32_IP%", WiFi.localIP().toString());
    page.replace("%ESP32_HOST%", "powerswitch-v3.local");
    server.send(200, "text/html", page);
  });
  
  // API: Get system status
  server.on("/api/status", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
    
    // Check API key
    if (server.hasHeader("X-API-Key")) {
      String apiKey = server.header("X-API-Key");
      if (apiKey != "power-switch-v3-2024") {
        server.send(401, "application/json", "{\"error\":\"Invalid API key\"}");
        return;
      }
    }
    
    StaticJsonDocument<1024> doc;
    
    // Sensor data
    doc["voltage1"] = sensors.voltage1;
    doc["voltage2"] = sensors.voltage2;
    doc["current"] = sensors.current;
    doc["power"] = sensors.power;
    doc["totalEnergy"] = totalEnergy;
    doc["dailyEnergy"] = dailyEnergy;
    doc["monthlyEnergy"] = monthlyEnergy;
    doc["temperature"] = sensors.temperature;
    
    // System state
    doc["systemState"] = getStateText();
    doc["autoMode"] = autoMode;
    doc["maintenanceMode"] = maintenanceMode;
    doc["notificationEnabled"] = notificationEnabled;
    doc["uptime"] = millis() / 1000;
    doc["wifiRSSI"] = WiFi.RSSI();
    doc["ipAddress"] = WiFi.localIP().toString();
    doc["switchCount"] = stats.switchCount;
    doc["faultCount"] = stats.faultCount;
    doc["totalUptime"] = stats.totalUptime + (millis() / 1000);
    
    // History data
    JsonArray v1Hist = doc.createNestedArray("voltageHistory1");
    JsonArray v2Hist = doc.createNestedArray("voltageHistory2");
    JsonArray iHist = doc.createNestedArray("currentHistory");
    JsonArray pHist = doc.createNestedArray("powerHistory");
    
    for (int i = 0; i < min(30, DATA_BUFFER_SIZE); i++) {
      int idx = (historyIndex - 1 - i + DATA_BUFFER_SIZE) % DATA_BUFFER_SIZE;
      v1Hist.add(voltageHistory1[idx]);
      v2Hist.add(voltageHistory2[idx]);
      iHist.add(currentHistory[idx]);
      pHist.add(powerHistory[idx]);
    }
    
    // Time info
    doc["currentTime"] = getTimeString();
    doc["currentDate"] = getDateString();
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
  });
  
  // API: Control system
  server.on("/api/control", HTTP_POST, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
    
    // Check API key
    if (server.hasHeader("X-API-Key")) {
      String apiKey = server.header("X-API-Key");
      if (apiKey != "power-switch-v3-2024") {
        server.send(401, "application/json", "{\"error\":\"Invalid API key\"}");
        return;
      }
    }
    
    if (server.hasArg("plain")) {
      StaticJsonDocument<200> doc;
      deserializeJson(doc, server.arg("plain"));
      
      String action = doc["action"];
      bool success = false;
      String message = "";
      
      if (action == "off" || action == "emergency") {
        digitalWrite(RELAY1_PIN, HIGH);
        digitalWrite(RELAY2_PIN, HIGH);
        systemState = STATE_OFF;
        success = true;
        message = "System turned OFF";
        if (action == "emergency") {
          message = "EMERGENCY STOP activated!";
          addSystemLog("CRITICAL", "Control", "Emergency stop activated via web");
        }
      } 
      else if (action == "source1") {
        if (!maintenanceMode && sensors.voltage1 > 100) {
          digitalWrite(RELAY1_PIN, LOW);
          digitalWrite(RELAY2_PIN, HIGH);
          systemState = STATE_SOURCE1_ACTIVE;
          stats.switchCount++;
          success = true;
          message = "Switched to Source 1";
          addSystemLog("INFO", "Control", "Manual switch to Source 1");
        } else {
          message = "Cannot switch: Maintenance mode active or no voltage";
        }
      }
      else if (action == "source2") {
        if (!maintenanceMode && sensors.voltage2 > 100) {
          digitalWrite(RELAY1_PIN, HIGH);
          digitalWrite(RELAY2_PIN, LOW);
          systemState = STATE_SOURCE2_ACTIVE;
          stats.switchCount++;
          success = true;
          message = "Switched to Source 2";
          addSystemLog("INFO", "Control", "Manual switch to Source 2");
        } else {
          message = "Cannot switch: Maintenance mode active or no voltage";
        }
      }
      else if (action == "toggle-mode") {
        autoMode = !autoMode;
        success = true;
        message = String("Mode changed to ") + (autoMode ? "AUTO" : "MANUAL");
        addSystemLog("INFO", "Control", "Mode toggled via API");
      }
      else if (action == "optimize") {
        success = true;
        message = "Optimization started";
        // Run in background
        performOptimization();
      }
      else if (action == "restart") {
        success = true;
        message = "System restarting";
        addSystemLog("INFO", "System", "Restart initiated via API");
      }
      
      StaticJsonDocument<100> response;
      response["success"] = success;
      response["message"] = message;
      response["autoMode"] = autoMode;
      
      String jsonResponse;
      serializeJson(response, jsonResponse);
      server.send(200, "application/json", jsonResponse);
      
      if (success && notificationEnabled) {
        sendTelegramAlert("🎮 *REMOTE CONTROL*\n" + message);
      }
    }
  });
  
  // API: GitHub specific status
  server.on("/api/github/status", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET");
    
    StaticJsonDocument<512> doc;
    
    doc["voltage1"] = sensors.voltage1;
    doc["voltage2"] = sensors.voltage2;
    doc["current"] = sensors.current;
    doc["power"] = sensors.power;
    doc["energy"] = totalEnergy;
    doc["temperature"] = sensors.temperature;
    doc["systemState"] = getStateText();
    doc["autoMode"] = autoMode;
    doc["switchCount"] = stats.switchCount;
    doc["uptime"] = millis() / 1000;
    doc["ipAddress"] = WiFi.localIP().toString();
    doc["wifiRSSI"] = WiFi.RSSI();
    doc["timestamp"] = timeClient.getEpochTime();
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
  });
  
  // API: Change mode
  server.on("/api/mode", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      StaticJsonDocument<200> doc;
      deserializeJson(doc, server.arg("plain"));
      
      String mode = doc["mode"];
      if (mode == "toggle") {
        autoMode = !autoMode;
      } else if (mode == "auto") {
        autoMode = true;
      } else if (mode == "manual") {
        autoMode = false;
      }
      
      preferences.begin("power-switch", false);
      preferences.putBool("autoMode", autoMode);
      preferences.end();
      
      StaticJsonDocument<100> response;
      response["success"] = true;
      response["autoMode"] = autoMode;
      response["message"] = "Mode updated";
      
      String jsonResponse;
      serializeJson(response, jsonResponse);
      server.send(200, "application/json", jsonResponse);
      
      String telegramMsg = "🔧 *MODE CHANGED*\n";
      telegramMsg += "New mode: " + String(autoMode ? "AUTO" : "MANUAL");
      sendTelegramAlert(telegramMsg);
      
      addSystemLog("INFO", "System", "Mode changed to " + String(autoMode ? "AUTO" : "MANUAL"));
    }
  });
  
  // API: Run optimization
  server.on("/api/optimize", HTTP_POST, []() {
    StaticJsonDocument<100> response;
    response["success"] = true;
    response["message"] = "Optimization started";
    
    String jsonResponse;
    serializeJson(response, jsonResponse);
    server.send(200, "application/json", jsonResponse);
    
    // Run optimization in background
    performOptimization();
  });
  
  // API: Get logs
  server.on("/api/logs", HTTP_GET, []() {
    StaticJsonDocument<8192> doc;
    JsonArray logs = doc.createNestedArray("logs");
    
    // Add system logs
    for (int i = 0; i < MAX_LOGS; i++) {
      int idx = (logIndex - 1 - i + MAX_LOGS) % MAX_LOGS;
      if (systemLogs[idx].timestamp > 0) {
        JsonObject log = logs.createNestedObject();
        log["timestamp"] = systemLogs[idx].timestamp;
        log["time"] = ctime((const time_t*)&systemLogs[idx].timestamp);
        log["level"] = systemLogs[idx].level;
        log["source"] = systemLogs[idx].source;
        log["message"] = systemLogs[idx].message;
      }
    }
    
    // Add system info
    doc["logCount"] = MAX_LOGS;
    doc["currentTime"] = timeClient.getEpochTime();
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
  });
  
  // API: Get system info
  server.on("/api/system/info", HTTP_GET, []() {
    StaticJsonDocument<512> doc;
    
    doc["chipId"] = ESP.getEfuseMac();
    doc["flashSize"] = ESP.getFlashChipSize();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["sketchSize"] = ESP.getSketchSize();
    doc["freeSketchSpace"] = ESP.getFreeSketchSpace();
    doc["cpuFreq"] = ESP.getCpuFreqMHz();
    doc["lastUpdate"] = preferences.getULong("lastUpdate", 0);
    doc["firmwareVersion"] = "3.0.1";
    doc["developer"] = "mh_alliwa";
    doc["ssid"] = WiFi.SSID();
    doc["rssi"] = WiFi.RSSI();
    doc["macAddress"] = WiFi.macAddress();
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
  });
  
  // API: Backup system
  server.on("/api/backup", HTTP_GET, []() {
    StaticJsonDocument<2048> doc;
    
    // System data
    doc["totalEnergy"] = totalEnergy;
    doc["dailyEnergy"] = dailyEnergy;
    doc["monthlyEnergy"] = monthlyEnergy;
    doc["switchCount"] = stats.switchCount;
    doc["faultCount"] = stats.faultCount;
    doc["totalUptime"] = stats.totalUptime;
    
    // Optimization data
    doc["zeroV1"] = optim.zeroV1;
    doc["zeroV2"] = optim.zeroV2;
    doc["zeroI"] = optim.zeroI;
    doc["calV1"] = optim.calibrationFactorV1;
    doc["calV2"] = optim.calibrationFactorV2;
    doc["calI"] = optim.calibrationFactorI;
    
    // Settings
    doc["autoMode"] = autoMode;
    doc["maintenanceMode"] = maintenanceMode;
    doc["notificationEnabled"] = notificationEnabled;
    doc["dataLogging"] = dataLogging;
    doc["cloudSync"] = cloudSync;
    
    // Alert thresholds
    doc["voltageLow"] = voltageLowThreshold;
    doc["voltageHigh"] = voltageHighThreshold;
    doc["currentHigh"] = currentHighThreshold;
    doc["temperatureHigh"] = temperatureThreshold;
    
    // Timestamp
    doc["backupTime"] = timeClient.getEpochTime();
    doc["backupVersion"] = "3.0";
    doc["deviceName"] = "PowerSwitch-V3";
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
  });
  
  // API: Restart system
  server.on("/api/restart", HTTP_POST, []() {
    StaticJsonDocument<100> response;
    response["success"] = true;
    response["message"] = "System restarting...";
    
    String jsonResponse;
    serializeJson(response, jsonResponse);
    server.send(200, "application/json", jsonResponse);
    
    delay(1000);
    ESP.restart();
  });
  
  // API: Factory reset
  server.on("/api/factory-reset", HTTP_POST, []() {
    preferences.begin("power-switch", false);
    preferences.clear();
    preferences.end();
    
    StaticJsonDocument<100> response;
    response["success"] = true;
    response["message"] = "Factory reset complete. Restarting...";
    
    String jsonResponse;
    serializeJson(response, jsonResponse);
    server.send(200, "application/json", jsonResponse);
    
    delay(2000);
    ESP.restart();
  });
  
  // OTA Update endpoint
  server.on("/update", HTTP_GET, []() {
    String html = "<form method='POST' action='/update' enctype='multipart/form-data'>";
    html += "<input type='file' name='update'>";
    html += "<input type='submit' value='Update'>";
    html += "</form>";
    server.send(200, "text/html", html);
  });
  
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("Update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %u\nRebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
  
  server.onNotFound([]() {
    server.send(404, "text/plain", "404: Not Found");
  });
  
  server.begin();
  Serial.println("✅ Web server started!");
  Serial.println("📱 Basic Interface: http://" + WiFi.localIP().toString());
  Serial.println("🌐 GitHub Interface: http://" + WiFi.localIP().toString() + "/github");
  Serial.println("🔧 OTA Update: http://" + WiFi.localIP().toString() + "/update");
  Serial.println("🔗 mDNS: http://powerswitch-v3.local");
}

// ========== SETUP FUNCTION ==========
void setup() {
  Serial.begin(115200);
  Serial.println("\n\n═══════════════════════════════════════════");
  Serial.println("       POWER SWITCH SYSTEM V3.0.1");
  Serial.println("       Advanced Power Management");
  Serial.println("       Developer: mh_alliwa");
  Serial.println("       Telegram: @mh_alliwa");
  Serial.println("       Instagram: @mh.alliwa");
  Serial.println("═══════════════════════════════════════════\n");
  
  // Initialize hardware
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_STATUS, OUTPUT);
  pinMode(BUTTON_MANUAL, INPUT_PULLUP);
  pinMode(TEMP_SENSOR_PIN, INPUT);
  
  // Initialize relays to OFF
  digitalWrite(RELAY1_PIN, HIGH);
  digitalWrite(RELAY2_PIN, HIGH);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_STATUS, LOW);
  
  // Initialize I2C for LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  delay(100);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.print("POWER SWITCH V3");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  
  // Load system settings
  loadSystemSettings();
  
  // Initialize statistics
  stats.startupTime = millis();
  
  // Add startup log
  addSystemLog("INFO", "System", "System booting...");
  
  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  
  lcd.setCursor(0, 1);
  lcd.print("WiFi Connecting");
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(timeout % 16, 0);
    lcd.print(".");
    timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.println("📡 IP Address: " + WiFi.localIP().toString());
    
    lcd.clear();
    lcd.print("WiFi Connected!");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP().toString());
    
    // Initialize services
    client.setInsecure();
    Blynk.config(BLYNK_AUTH_TOKEN);
    timeClient.begin();
    timeClient.update();
    
    // Initialize web server
    initWebServer();
    
    // Send startup notification
    String startupMsg = "🚀 *POWER SWITCH V3 ONLINE*\n\n";
    startupMsg += "📍 IP: " + WiFi.localIP().toString() + "\n";
    startupMsg += "🔧 Mode: " + String(autoMode ? "AUTO" : "MANUAL") + "\n";
    startupMsg += "📊 Status: " + getStateText() + "\n";
    startupMsg += "⚡ Energy: " + String(totalEnergy, 2) + " kWh\n";
    startupMsg += "🔄 Switches: " + String(stats.switchCount) + "\n";
    startupMsg += "📶 RSSI: " + String(WiFi.RSSI()) + " dBm\n";
    startupMsg += "━━━━━━━━━━━━━━━━━━\n";
    startupMsg += "System ready for operation\n";
    startupMsg += "Developer: @mh_alliwa";
    
    sendTelegramAlert(startupMsg);
    addSystemLog("INFO", "System", "Startup complete. System online.");
    
    delay(2000);
  } else {
    Serial.println("\n❌ WiFi Connection Failed!");
    lcd.clear();
    lcd.print("WiFi FAILED!");
    lcd.setCursor(0, 1);
    lcd.print("Offline Mode");
    
    addSystemLog("WARNING", "System", "WiFi connection failed. Running in offline mode.");
  }
  
  // Setup timers
  timer.setInterval(500L, []() {
    if (WiFi.status() == WL_CONNECTED) {
      Blynk.virtualWrite(V0, sensors.voltage1);
      Blynk.virtualWrite(V1, sensors.voltage2);
      Blynk.virtualWrite(V2, sensors.current);
      Blynk.virtualWrite(V3, sensors.power);
      Blynk.virtualWrite(V4, totalEnergy);
      Blynk.virtualWrite(V5, WiFi.RSSI());
      Blynk.virtualWrite(V6, sensors.temperature);
      Blynk.virtualWrite(V7, autoMode ? 1 : 0);
    }
  });
  
  // Telegram update timer
  timer.setInterval(2000L, []() {
    if (WiFi.status() == WL_CONNECTED) {
      handleTelegramMessages();
    }
  });
  
  // System save timer (every 5 minutes)
  timer.setInterval(300000L, []() {
    saveSystemSettings();
    Serial.println("💾 System settings saved");
  });
  
  // Initial LCD update
  lcd.clear();
  lcd.print("SYSTEM READY");
  lcd.setCursor(0, 1);
  lcd.print("V3.0 - mh_alliwa");
  
  delay(2000);
  
  Serial.println("\n✅ System initialization complete!");
  Serial.println("──────────────────────────────────────");
}

// ========== MAIN LOOP ==========
void loop() {
  // Run Blynk
  if (WiFi.status() == WL_CONNECTED) {
    Blynk.run();
  }
  
  // Run timers
  timer.run();
  
  // Handle web server
  server.handleClient();
  
  // Update sensors
  readSensors();
  
  // Update time client
  if (WiFi.status() == WL_CONNECTED) {
    timeClient.update();
  }
  
  // Perform auto switching if enabled
  performAutoSwitch();
  
  // Update LCD display
  updateLCD();
  
  // Handle manual button
  static unsigned long lastButtonPress = 0;
  if (!digitalRead(BUTTON_MANUAL) && millis() - lastButtonPress > 1000) {
    lastButtonPress = millis();
    
    if (systemState == STATE_OFF && sensors.voltage1 > 100) {
      digitalWrite(RELAY1_PIN, LOW);
      digitalWrite(RELAY2_PIN, HIGH);
      systemState = STATE_SOURCE1_ACTIVE;
      stats.switchCount++;
      
      String msg = "🔘 *MANUAL BUTTON*\n";
      msg += "System started via physical button\n";
      msg += "Source: 1 (" + String(sensors.voltage1, 1) + "V)";
      sendTelegramAlert(msg);
      
      addSystemLog("INFO", "Button", "System started via button (Source 1)");
    } else if (systemState != STATE_OFF) {
      digitalWrite(RELAY1_PIN, HIGH);
      digitalWrite(RELAY2_PIN, HIGH);
      systemState = STATE_OFF;
      
      String msg = "🔘 *MANUAL BUTTON*\n";
      msg += "System stopped via physical button";
      sendTelegramAlert(msg);
      
      addSystemLog("INFO", "Button", "System stopped via button");
    }
  }
  
  // Watchdog reset
  esp_task_wdt_reset();
  
  // Small delay to prevent watchdog timeout
  delay(10);
}
