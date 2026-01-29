# Power Switch System V3

## 📋 Overview
Advanced dual source power management system with ESP32, featuring three web interfaces:
1. **Basic Web Interface** - Local ESP32 interface
2. **Advanced Web Interface** - Local dashboard with real-time monitoring
3. **GitHub Pages Dashboard** - Remote monitoring and control

## 🚀 Features

### Hardware Features
- Dual power source monitoring (Source 1 & Source 2)
- Real-time voltage, current, and power measurement
- Automatic source switching based on voltage thresholds
- Temperature monitoring and safety shutdown
- Manual override with physical button
- LCD display for local status

### Software Features
- **3 Web Interfaces**: Local + GitHub Pages
- **Telegram Bot**: Remote control and notifications
- **Blynk Integration**: Mobile app control
- **OTA Updates**: Wireless firmware updates
- **Data Logging**: Historical data storage
- **Auto Optimization**: Self-calibration system
- **Safety Systems**: Over-voltage, over-current, over-temperature protection

## 🛠️ Installation

### ESP32 Setup
1. Install Arduino IDE with ESP32 support
2. Install required libraries:
   - Wire
   - LiquidCrystal_I2C
   - WiFi
   - UniversalTelegramBot
   - Blynk
   - ArduinoJson
   - WebServer
   - Preferences

3. Upload `power_switch_v3.ino` to ESP32
4. Configure Wi-Fi credentials in code
5. Set up Telegram Bot token and Chat ID

### GitHub Pages Setup
1. Create new repository: `power-switch-dashboard`
2. Upload all files to repository
3. Enable GitHub Pages in repository settings
4. Set source to `main` branch
5. Access dashboard at: `https://[username].github.io/power-switch-dashboard`

## 📡 Network Configuration

### Local Access
- ESP32 IP: Check serial monitor for assigned IP
- Basic Interface: `http://[ESP32_IP]/`
- Advanced Interface: `http://[ESP32_IP]/github`
- mDNS: `http://powerswitch-v3.local`

### Remote Access
- GitHub Pages: `https://[username].github.io/power-switch-dashboard`
- The dashboard will automatically try to connect to ESP32 on local network

## 🎯 Usage

### Basic Controls
1. **Auto Mode**: System automatically switches between sources
2. **Manual Mode**: Manual control via web interface or Telegram
3. **Emergency Stop**: Immediate shutdown of all relays
4. **Optimization**: Run calibration for improved accuracy

### Monitoring
- Real-time voltage graphs
- Power consumption analytics
- Energy usage tracking
- System logs and alerts
- Network status

### Alerts & Notifications
- Telegram alerts for system events
- Email notifications (if configured)
- Visual alerts on dashboard
- Sound alerts via buzzer

## 🔧 Configuration

### Sensor Calibration
1. Run `/optimize` command via Telegram
2. Use web interface optimization button
3. Manual calibration via preferences

### Threshold Settings
- Voltage low threshold: 170V - 200V
- Voltage high threshold: 240V - 260V
- Current limit: 5A - 30A
- Temperature limit: 50°C - 70°C

## 🚨 Emergency Features

### Automatic Protection
1. **Over-voltage Protection**: Shuts down if voltage > 250V
2. **Over-current Protection**: Shuts down if current > 20A
3. **Over-temperature Protection**: Shuts down if temperature > 60°C
4. **Low Voltage Warning**: Alerts when voltage < 180V

### Manual Override
- Physical emergency button
- Telegram emergency command
- Web interface emergency stop
- LCD display shows fault status

## 📊 Data Management

### Storage
- Local storage using Preferences
- Historical data buffering
- System logs (100 entries)
- Energy consumption tracking

### Export
- JSON data export via web interface
- System backup/restore
- Log download

## 🔐 Security

### Authentication
- API key protection
- Telegram chat ID verification
- Local network access only (by default)

### Network Security
- HTTPS for GitHub Pages
- Local HTTP with API key
- Secure WebSocket connections

## 🐛 Troubleshooting

### Common Issues

1. **Wi-Fi Connection Failed**
   - Check SSID and password
   - Verify ESP32 Wi-Fi capability
   - Check router settings

2. **Web Interface Not Loading**
   - Check ESP32 IP address
   - Verify mDNS setup
   - Check firewall settings

3. **Sensors Not Reading**
   - Verify wiring connections
   - Run optimization routine
   - Check power supply

4. **GitHub Pages Not Connecting**
   - Ensure ESP32 and browser are on same network
   - Check ESP32 IP in dashboard settings
   - Verify CORS settings

### Debugging
- Serial monitor output (115200 baud)
- System logs in web interface
- Telegram bot debug messages
- LCD display status codes

## 📈 Performance

### System Requirements
- ESP32 with WiFi
- 4MB Flash (minimum)
- Stable 5V power supply
- Reliable Wi-Fi connection

### Expected Performance
- Sensor updates: 2 times/second
- Web interface updates: 1 time/second
- Telegram response: < 2 seconds
- Switching delay: 100ms

## 🤝 Contributing

1. Fork the repository
2. Create feature branch
3. Commit changes
4. Push to branch
5. Create Pull Request

## 📄 License
MIT License - See LICENSE file for details

## 👨‍💻 Developer
**mh_alliwa**
- Telegram: @mh_alliwa
- Instagram: @mh.alliwa
- GitHub: mh-alliwa

## 🙏 Acknowledgments
- ESP32 community for excellent libraries
- Blynk for IoT platform
- Chart.js for visualization
- All contributors and testers

---

**⚠️ Safety Warning**: This system controls high voltage electricity. Proper electrical safety precautions must be followed. The developer is not responsible for any damage or injury resulting from improper use.
