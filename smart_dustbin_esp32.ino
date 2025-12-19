#include <ESP32Servo.h>
#include <WiFi.h>
#include <WebServer.h>

// ------------------- WiFi CONFIG -------------------
const char* ssid     = "Avee";
const char* password = "12345678";

// ------------------- WEB SERVER --------------------
WebServer server(80);

// ------------------- SERVO & PINS ------------------
Servo servo1;

// Sensor Pins
#define IR_PIN         18   // Dry (IR Sensor)
#define PROXIMITY_PIN  19   // Metal (Inductive Sensor)
#define BUZZER_PIN     21
#define SERVO_PIN      13
#define RAINDROP_PIN   23   // Wet (Rain Sensor)

// Stepper Pins (ULN2003)
#define STEPPER_PIN1   25
#define STEPPER_PIN2   26
#define STEPPER_PIN3   27
#define STEPPER_PIN4   14

// Ultrasonic Pins
#define TRIG_METAL     2
#define ECHO_METAL     15
#define TRIG_WET       16
#define ECHO_WET       17
#define TRIG_DRY       4
#define ECHO_DRY       5

// ------------------- BIN CONFIG --------------------
const int BIN_HEIGHT         = 12;  // cm
const int OVERFLOW_THRESHOLD = 2;   // near full when <= 2 cm
const int FULL_THRESHOLD     = 2;   // full when <= 2 cm

// ------------------- STEPPER CONFIG (HIGH SPEED) ---
// HIGH SPEED CONFIGURATION
// We switched to Full-Step mode. Steps per rev is half of 2038 = ~1019
const int stepsPerRevolution = 1019; 

// Delay between steps (Lower = Faster). 
// 3ms is very fast. If it stalls/hums, change to 4 or 5.
const int stepDelay          = 3;   

const int HOME_POS  = 0;    // Home position (Metal bin)
const int DRY_POS   = 240;  // 240 degrees clockwise
const int WET_POS   = -240; // 240 degrees counter-clockwise

int currentStepperDegree = HOME_POS;

// NEW STEP SEQUENCE: Full Step (High Torque, High Speed)
// 4 Steps instead of 8. Powers two coils at a time.
int stepSequence[4][4] = {
  {1, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 1},
  {1, 0, 0, 1}
};

// ------------------- STATE -------------------------
bool proximityTriggered = false;
bool irTriggered        = false;
bool raindropTriggered  = false;

unsigned long lastProximityTime  = 0;
unsigned long lastIRTime         = 0;
unsigned long lastRaindropTime   = 0;
unsigned long lastFillCheckTime  = 0;

// Cooldowns
const unsigned long PROXIMITY_COOLDOWN = 3000; 
const unsigned long IR_COOLDOWN        = 2000; 
const unsigned long RAINDROP_COOLDOWN  = 3000; 
const unsigned long FILL_CHECK_INTERVAL = 5000;

// Bin levels
int metalFillLevel = 0;
int wetFillLevel   = 0;
int dryFillLevel   = 0;

// Counters
unsigned long metalCount    = 0;
unsigned long wetCount      = 0;
unsigned long dryCount      = 0;
unsigned long totalCount    = 0;
unsigned long rejectedCount = 0;

// Dashboard state
bool   overflowFlag   = false;
String currentWaste   = "None";

// ------------------- HTML PAGE ---------------------
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Dustbin Monitor</title>
<style>
*{margin:0;padding:0;box-sizing:border-box;}
body{font-family:'Segoe UI',Tahoma,Geneva,Verdana,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:20px;color:#333;}
.container{max-width:1200px;margin:0 auto;}
.header{background:#fff;padding:30px;border-radius:15px;margin-bottom:20px;box-shadow:0 10px 30px rgba(0,0,0,0.3);text-align:center;}
.header h1{color:#667eea;font-size:2.5em;margin-bottom:10px;}
.status{display:inline-flex;align-items:center;gap:10px;padding:10px 20px;background:#f0f0f0;border-radius:25px;margin-top:15px;}
.status-dot{width:12px;height:12px;border-radius:50%;background:#ff4444;animation:pulse 2s infinite;}
.status-dot.connected{background:#44ff44;}
@keyframes pulse{0%,100%{opacity:1;}50%{opacity:0.5;}}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin-bottom:20px;}
.bin-card{background:#fff;border-radius:15px;padding:25px;box-shadow:0 10px 30px rgba(0,0,0,0.2);transition:transform 0.3s;}
.bin-card:hover{transform:translateY(-5px);}
.bin-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px;}
.bin-title{font-size:1.5em;font-weight:bold;}
.bin-icon{font-size:2.5em;}
.metal{color:#607d8b;}
.wet{color:#2196F3;}
.dry{color:#ff9800;}
.fill-container{position:relative;height:150px;background:#e0e0e0;border-radius:10px;overflow:hidden;margin:20px 0;}
.fill-level{position:absolute;bottom:0;width:100%;transition:height .5s ease;display:flex;align-items:center;justify-content:center;color:#fff;font-weight:bold;font-size:1.3em;}
.fill-metal{background:linear-gradient(to top,#37474f,#78909c);}
.fill-wet{background:linear-gradient(to top,#1565c0,#42a5f5);}
.fill-dry{background:linear-gradient(to top,#e65100,#ff9800);}
.stat-row{display:flex;justify-content:space-between;padding:10px 0;border-bottom:1px solid #eee;}
.stat-label{color:#666;}
.stat-value{font-weight:bold;color:#333;font-size:1.1em;}
.stats-panel{background:#fff;border-radius:15px;padding:25px;box-shadow:0 10px 30px rgba(0,0,0,0.2);margin-bottom:20px;}
.stats-panel h2{color:#667eea;margin-bottom:20px;font-size:1.8em;}
.stats-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:15px;}
.stat-box{background:linear-gradient(135deg,#667eea,#764ba2);color:#fff;padding:20px;border-radius:10px;text-align:center;}
.stat-box h3{font-size:2.5em;margin-bottom:5px;}
.stat-box p{opacity:.9;font-size:.9em;}
.alert{background:#ff5252;color:#fff;padding:20px;border-radius:10px;margin-bottom:20px;display:none;animation:shake .5s;}
.alert.show{display:block;}
@keyframes shake{0%,100%{transform:translateX(0);}25%{transform:translateX(-10px);}75%{transform:translateX(10px);}}
.loading{text-align:center;padding:40px;color:#fff;font-size:1.2em;}

#currentWasteDisplay {
    margin-top: 10px;
    background-color: #ffeb3b;
    color: #333;
    padding: 10px 20px;
    border-radius: 10px;
    font-size: 1.8em;
    font-weight: bold;
    display: inline-block;
    box-shadow: 0 4px 10px rgba(0,0,0,0.15);
}
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <h1>🗑 Smart Dustbin System </h1>
    <p style="color:#666;">Real-Time Waste Segregation System</p>
    <div class="status">
      <div class="status-dot" id="statusDot"></div>
      <span id="statusText">Connecting...</span>
    </div>
    <div style="margin-top:10px;color:#666;">
      <small>Uptime: <strong id="uptime">--</strong></small>
    </div>
    
    <div id="currentWasteDisplay">
      Current Waste: <strong id="currentWaste">None</strong>
    </div>
    </div>

  <div class="alert" id="alert">
    <strong>⚠ OVERFLOW ALERT!</strong><br>
    One or more bins are nearly full. Please empty them soon.
  </div>

  <div class="grid">
    <div class="bin-card">
      <div class="bin-header">
        <div class="bin-title metal">Metal Bin</div>
        <div class="bin-icon">🔩</div>
      </div>
      <div class="fill-container">
        <div class="fill-level fill-metal" id="metalFill" style="height:0%;">
          <span id="metalPercent">0%</span>
        </div>
      </div>
      <div class="stat-row">
        <span class="stat-label">Items:</span>
        <span class="stat-value" id="metalCount">0</span>
      </div>
      <div class="stat-row">
        <span class="stat-label">Space:</span>
        <span class="stat-value" id="metalSpace">--</span>
      </div>
    </div>

    <div class="bin-card">
      <div class="bin-header">
        <div class="bin-title wet">Wet Bin</div>
        <div class="bin-icon">💧</div>
      </div>
      <div class="fill-container">
        <div class="fill-level fill-wet" id="wetFill" style="height:0%;">
          <span id="wetPercent">0%</span>
        </div>
      </div>
      <div class="stat-row">
        <span class="stat-label">Items:</span>
        <span class="stat-value" id="wetCount">0</span>
      </div>
      <div class="stat-row">
        <span class="stat-label">Available Space:</span>
        <span class="stat-value" id="wetSpace">--</span>
      </div>
    </div>

    <div class="bin-card">
      <div class="bin-header">
        <div class="bin-title dry">Dry Bin</div>
        <div class="bin-icon">📄</div>
      </div>
      <div class="fill-container">
        <div class="fill-level fill-dry" id="dryFill" style="height:0%;">
          <span id="dryPercent">0%</span>
        </div>
      </div>
      <div class="stat-row">
        <span class="stat-label">Items:</span>
        <span class="stat-value" id="dryCount">0</span>
      </div>
      <div class="stat-row">
        <span class="stat-label">Space:</span>
        <span class="stat-value" id="drySpace">--</span>
      </div>
    </div>
  </div>

  <div class="stats-panel">
    <h2>📊 Statistics</h2>
    <div class="stats-grid">
      <div class="stat-box">
        <h3 id="totalWaste">0</h3>
        <p>Total Processed</p>
      </div>
      <div class="stat-box">
        <h3 id="rejected">0</h3>
        <p>Rejected</p>
      </div>
      <div class="stat-box">
        <h3 id="efficiency">100%</h3>
        <p>Efficiency</p>
      </div>
    </div>
  </div>
</div>

<script>
function formatUptime(seconds){
  var h = Math.floor(seconds/3600);
  var m = Math.floor((seconds%3600)/60);
  var s = seconds%60;
  return h+"h "+m+"m "+s+"s";
}

function updateUI(d){
  var dot = document.getElementById('statusDot');
  var statusText = document.getElementById('statusText');
  dot.classList.add('connected');
  statusText.textContent = 'Online';

  document.getElementById('uptime').textContent = formatUptime(d.uptime || 0);
  document.getElementById('currentWaste').textContent = d.currentWaste || 'None';

  // Metal
  document.getElementById('metalFill').style.height = (d.metalPercent||0)+'%';
  document.getElementById('metalPercent').textContent = (d.metalPercent||0)+'%';
  document.getElementById('metalCount').textContent = d.metalCount || 0;
  document.getElementById('metalSpace').textContent = (d.metalSpace||0)+' cm';

  // Wet
  document.getElementById('wetFill').style.height = (d.wetPercent||0)+'%';
  document.getElementById('wetPercent').textContent = (d.wetPercent||0)+'%';
  document.getElementById('wetCount').textContent = d.wetCount || 0;
  document.getElementById('wetSpace').textContent = (d.wetSpace||0)+' cm';

  // Dry
  document.getElementById('dryFill').style.height = (d.dryPercent||0)+'%';
  document.getElementById('dryPercent').textContent = (d.dryPercent||0)+'%';
  document.getElementById('dryCount').textContent = d.dryCount || 0;
  document.getElementById('drySpace').textContent = (d.drySpace||0)+' cm';

  // Stats
  var total = d.totalWaste || 0;
  var rejected = d.rejectedWaste || 0;
  document.getElementById('totalWaste').textContent = total;
  document.getElementById('rejected').textContent = rejected;
  var eff = total>0 ? Math.round(((total-rejected)/total)*100) : 100;
  document.getElementById('efficiency').textContent = eff + '%';

  // Overflow alert
  var alertEl = document.getElementById('alert');
  if(d.overflow){ alertEl.classList.add('show'); }
  else{ alertEl.classList.remove('show'); }
}

function fetchData(){
  fetch('/data')
    .then(r => r.json())
    .then(updateUI)
    .catch(e => {
      var dot = document.getElementById('statusDot');
      var statusText = document.getElementById('statusText');
      dot.classList.remove('connected');
      statusText.textContent = 'Disconnected';
    });
}

setInterval(fetchData, 1000);
window.onload = fetchData;
</script>
</body>
</html>
)rawliteral";

// ------------------- FUNCTION PROTOTYPES -----------
void connectWiFi();
void updateBinLevels();
void checkOverflow();
void displayBinStatus();
void playBuzzer(int freq, int duration);
int  measureDistance(int trig, int echo);
void moveStepperToPosition(int target);
void moveStepperSteps(int steps, bool clockwise);
void rejectWaste();
int  calculateFillPercentage(int distance);
bool isBinFull(int distance);
void handleRoot();
void handleData();

// ------------------- SETUP -------------------------
void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n=== SMART DUSTBIN SYSTEM (FAST MODE) ===");

  pinMode(PROXIMITY_PIN, INPUT_PULLUP);
  pinMode(IR_PIN, INPUT);
  pinMode(RAINDROP_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(STEPPER_PIN1, OUTPUT);
  pinMode(STEPPER_PIN2, OUTPUT);
  pinMode(STEPPER_PIN3, OUTPUT);
  pinMode(STEPPER_PIN4, OUTPUT);

  pinMode(TRIG_METAL, OUTPUT);
  pinMode(ECHO_METAL, INPUT);
  pinMode(TRIG_WET, OUTPUT);
  pinMode(ECHO_WET, INPUT);
  pinMode(TRIG_DRY, OUTPUT);
  pinMode(ECHO_DRY, INPUT);

  servo1.attach(SERVO_PIN);
  servo1.write(70); // lid closed

  playBuzzer(1000, 150);
  playBuzzer(1500, 150);

  updateBinLevels();
  displayBinStatus();

  connectWiFi();

  // Web server routes
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("HTTP server started.");

  Serial.println("Open in browser: http://" + WiFi.localIP().toString());
}

// ------------------- LOOP --------------------------
void loop() {
  server.handleClient();

  unsigned long now = millis();

  if (now - lastFillCheckTime > FILL_CHECK_INTERVAL) {
    updateBinLevels();
    checkOverflow();
    lastFillCheckTime = now;
  }

  int proximityState = digitalRead(PROXIMITY_PIN);
  int irState        = digitalRead(IR_PIN);
  int raindropState  = digitalRead(RAINDROP_PIN);

  // METAL - STAY AT HOME POSITION
  if (proximityState == 0 && !proximityTriggered &&
      (now - lastProximityTime > PROXIMITY_COOLDOWN)) {

    Serial.println("\n🔩 METAL WASTE DETECTED");
    if (isBinFull(metalFillLevel)) {
      Serial.println("Bin Full → REJECTED");
      rejectWaste();
      rejectedCount++;
      currentWaste = "Metal (Rejected)";
    } else {
      proximityTriggered = true;
      lastProximityTime = now;

      playBuzzer(1000, 500);
      moveStepperToPosition(HOME_POS);
      servo1.write(180);
      delay(2000);
      servo1.write(70);

      metalCount++;
      totalCount++;
      currentWaste = "Metal";
      playBuzzer(1500, 150);
      displayBinStatus();
    }
  }
  if (proximityState == 1) proximityTriggered = false;

  // DRY - ROTATE 240° CLOCKWISE
  if (irState == 0 && !irTriggered && (now - lastIRTime > IR_COOLDOWN)) {

    Serial.println("\n📄 DRY WASTE DETECTED");
    if (isBinFull(dryFillLevel)) {
      Serial.println("Bin Full → REJECTED");
      rejectWaste();
      rejectedCount++;
      currentWaste = "Dry (Rejected)";
    } else {
      irTriggered = true;
      lastIRTime = now;

      playBuzzer(1000, 500);
      moveStepperToPosition(DRY_POS);
      servo1.write(180);
      delay(2000);
      servo1.write(70);
      moveStepperToPosition(HOME_POS);

      dryCount++;
      totalCount++;
      currentWaste = "Dry";
      playBuzzer(1500, 150);
      displayBinStatus();
    }
  }
  if (irState == 1) irTriggered = false;

  // WET - ROTATE 240° COUNTER-CLOCKWISE
  if (raindropState == 0 && !raindropTriggered &&
      (now - lastRaindropTime > RAINDROP_COOLDOWN)) {

    Serial.println("\n💧 WET WASTE DETECTED");
    if (isBinFull(wetFillLevel)) {
      Serial.println("Bin Full → REJECTED");
      rejectWaste();
      rejectedCount++;
      currentWaste = "Wet (Rejected)";
    } else {
      raindropTriggered = true;
      lastRaindropTime = now;

      playBuzzer(1000, 500);
      moveStepperToPosition(WET_POS);
      servo1.write(180);
      delay(2000);
      servo1.write(70);
      moveStepperToPosition(HOME_POS);

      wetCount++;
      totalCount++;
      currentWaste = "Wet";
      playBuzzer(1500, 150);
      displayBinStatus();
    }
  }
  if (raindropState == 1) raindropTriggered = false;

  delay(100);
}

// ------------------- WIFI --------------------------
void connectWiFi() {
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    Serial.print(".");
    delay(250);
    tries++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("WiFi Failed (still starting web server, but no IP).");
  }
}

// ------------------- ULTRASONIC & BINS ------------
void updateBinLevels() {
  metalFillLevel = measureDistance(TRIG_METAL, ECHO_METAL);
  wetFillLevel   = measureDistance(TRIG_WET, ECHO_WET);
  dryFillLevel   = measureDistance(TRIG_DRY, ECHO_DRY);
}

int measureDistance(int trig, int echo) {
  digitalWrite(trig, LOW); delayMicroseconds(2);
  digitalWrite(trig, HIGH); delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long d = pulseIn(echo, HIGH, 30000);
  int cm = d * 0.034 / 2;
  if (cm == 0 || cm > BIN_HEIGHT) cm = BIN_HEIGHT;
  return cm;
}

int calculateFillPercentage(int distance) {
  return constrain(((BIN_HEIGHT - distance) * 100) / BIN_HEIGHT, 0, 100);
}

bool isBinFull(int distance) {
  return distance <= FULL_THRESHOLD;
}

void checkOverflow() {
  overflowFlag = false;

  if (metalFillLevel <= OVERFLOW_THRESHOLD) {
    Serial.println("⚠ WARNING: Metal bin near full!");
    overflowFlag = true;
  }
  if (wetFillLevel <= OVERFLOW_THRESHOLD) {
    Serial.println("⚠ WARNING: Wet bin near full!");
    overflowFlag = true;
  }
  if (dryFillLevel <= OVERFLOW_THRESHOLD) {
    Serial.println("⚠ WARNING: Dry bin near full!");
    overflowFlag = true;
  }

  if (overflowFlag) {
    playBuzzer(2500, 300);
    delay(100);
    playBuzzer(2500, 300);
  }
}

void displayBinStatus() {
  Serial.println("\n==== BIN STATUS ====");
  Serial.printf("METAL: %d%% (%dcm)\n", calculateFillPercentage(metalFillLevel), metalFillLevel);
  Serial.printf("WET:   %d%% (%dcm)\n", calculateFillPercentage(wetFillLevel), wetFillLevel);
  Serial.printf("DRY:   %d%% (%dcm)\n", calculateFillPercentage(dryFillLevel), dryFillLevel);
  Serial.printf("Total: %lu | Rejected: %lu\n", totalCount, rejectedCount);
  Serial.println("=====================\n");
}

// ------------------- BUZZER & STEPPER -------------
void rejectWaste() {
  for (int i = 0; i < 3; i++) {
    playBuzzer(2000, 200);
    delay(100);
  }
}

void playBuzzer(int freq, int duration) {
  tone(BUZZER_PIN, freq, duration);
  delay(duration);
  noTone(BUZZER_PIN);
}

// UPDATED: Cycles through 0-3 (4 steps) instead of 0-7
void moveStepperSteps(int steps, bool clockwise) {
  static int pos = 0;

  for (int i = 0; i < steps; i++) {
    pos += clockwise ? 1 : -1;
    if (pos > 3) pos = 0;
    if (pos < 0) pos = 3;

    digitalWrite(STEPPER_PIN1, stepSequence[pos][0]);
    digitalWrite(STEPPER_PIN2, stepSequence[pos][1]);
    digitalWrite(STEPPER_PIN3, stepSequence[pos][2]);
    digitalWrite(STEPPER_PIN4, stepSequence[pos][3]);

    delay(stepDelay);
  }
  
  // Power down coils to prevent heating
  digitalWrite(STEPPER_PIN1, LOW);
  digitalWrite(STEPPER_PIN2, LOW);
  digitalWrite(STEPPER_PIN3, LOW);
  digitalWrite(STEPPER_PIN4, LOW);
}

void moveStepperToPosition(int target) {
  int diff = target - currentStepperDegree;
  bool clockwise = diff > 0;
  
  // Updated stepsPerRevolution to 1019 for Full Step mode
  int steps = round(abs(diff) * (float)stepsPerRevolution / 360.0);

  moveStepperSteps(steps, clockwise);
  currentStepperDegree = target;
}


// ------------------- WEB HANDLERS ------------------
void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleData() {
  String json = "{";
  json += "\"uptime\":"        + String(millis()/1000) + ",";
  json += "\"currentWaste\":\""+ currentWaste + "\",";
  json += "\"metalPercent\":"  + String(calculateFillPercentage(metalFillLevel)) + ",";
  json += "\"wetPercent\":"    + String(calculateFillPercentage(wetFillLevel)) + ",";
  json += "\"dryPercent\":"    + String(calculateFillPercentage(dryFillLevel)) + ",";
  json += "\"metalSpace\":"    + String(metalFillLevel) + ",";
  json += "\"wetSpace\":"      + String(wetFillLevel) + ",";
  json += "\"drySpace\":"      + String(dryFillLevel) + ",";
  json += "\"metalCount\":"    + String(metalCount) + ",";
  json += "\"wetCount\":"      + String(wetCount) + ",";
  json += "\"dryCount\":"      + String(dryCount) + ",";
  json += "\"totalWaste\":"    + String(totalCount) + ",";
  json += "\"rejectedWaste\":" + String(rejectedCount) + ",";
  json += "\"overflow\":"      + String(overflowFlag ? "true" : "false");
  json += "}";

  server.send(200, "application/json", json);
}