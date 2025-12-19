 🗑 Smart Dustbin System (ESP32 Based)

An IoT-enabled Automatic Waste Segregation System** that classifies waste into Metal, Wet, and Dry categories using sensors and displays real-time data on a web dashboard.


 📌 Features
- Automatic waste detection & segregation
- Metal / Wet / Dry classification
- Stepper motor-based bin alignment
- Servo-controlled lid
- Ultrasonic-based bin level monitoring
- Real-time Web Dashboard (ESP32 Web Server)
- Overflow alert with buzzer
- Live statistics & efficiency tracking


 🔧 Hardware Components
- ESP32 Development Board
- Inductive Proximity Sensor (Metal)
- IR Sensor (Dry)
- Rain Sensor (Wet)
- Ultrasonic Sensors (HC-SR04 × 3)
- Stepper Motor + ULN2003 Driver
- Servo Motor
- Buzzer
- Power Supply


🧠 System Architecture
[Block Diagram](docs/block_diagram.png)

## ⚙ Software & Technologies
- Arduino (ESP32)
- Embedded C/C++
- HTML, CSS, JavaScript (Web Dashboard)
- WiFi & HTTP Server



🌐 Web Dashboard
- Live bin fill levels
- Waste type detection
- Item counters
- Overflow alerts
- System uptime

[Dashboard](web_ui/dashboard_preview.jpeg)

📂 Project Structure

🚀 How to Run
1. Open `smart_dustbin_esp32.ino` in Arduino IDE
2. Install ESP32 board support
3. Update WiFi credentials:

  cpp
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

4. Upload code to ESP32
5. Open Serial Monitor → copy IP address
6. Open browser → http://ESP32_IP

📊 Applications

Smart Cities
Waste Management Automation
University IoT Projects
Research & Prototypes

👨‍💻 Author

Tharusha Nethmina
Software Engineering Undergraduate