// config.js - Configuration for GitHub Pages Dashboard
const CONFIG = {
    // API Configuration
    API_KEY: 'power-switch-v3-2024',
    
    // ESP32 Endpoints (will be tried in order)
    ENDPOINTS: [
        'powerswitch-v3.local',
        '192.168.1.100',
        'esp32.local'
    ],
    
    // Update Intervals (ms)
    UPDATE_INTERVALS: {
        FAST: 1000,
        NORMAL: 2000,
        SLOW: 5000
    },
    
    // Chart Configuration
    CHART: {
        HISTORY_POINTS: 100,
        ANIMATION_DURATION: 1000,
        COLORS: {
            VOLTAGE1: '#4cc9f0',
            VOLTAGE2: '#f72585',
            CURRENT: '#2ecc71',
            POWER: '#f39c12',
            TEMPERATURE: '#e74c3c'
        }
    },
    
    // Thresholds
    THRESHOLDS: {
        VOLTAGE: {
            LOW: 180,
            HIGH: 250,
            OPTIMAL_MIN: 210,
            OPTIMAL_MAX: 230
        },
        CURRENT: {
            WARNING: 15,
            CRITICAL: 20
        },
        TEMPERATURE: {
            WARNING: 50,
            CRITICAL: 60
        }
    },
    
    // Demo Mode Settings
    DEMO: {
        ENABLED: true,
        UPDATE_INTERVAL: 2000,
        DATA_VARIATION: 0.1 // 10% variation
    },
    
    // UI Settings
    UI: {
        THEME: 'dark',
        ANIMATIONS: true,
        NOTIFICATIONS: true,
        AUTO_RECONNECT: true,
        RECONNECT_INTERVAL: 10000 // 10 seconds
    }
};

// Export configuration
if (typeof module !== 'undefined' && module.exports) {
    module.exports = CONFIG;
}
