#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/* ================= PIN ================= */
#define GAS_PIN     A0
#define BUZZER_PIN  12
#define LED_PIN     2

/* ================= WIFI + MQTT ================= */
const char* ssid = "Hieu Cute";
const char* password = "ximon123";
const char* mqtt_server = "broker.emqx.io";

const char* topic_status  = "node2/status";
const char* topic_control = "node2/control";
const char* topic_mode    = "node2/mode";  // Thêm topic nhận chế độ

/* ================= CONFIG ================= */
#define GAS_THRESHOLD 550
#define GAS_INTERVAL  50
#define STATUS_INTERVAL 80
#define MANUAL_TIMEOUT 600000  // 10 phút (600000 ms)

/* ================= STATE ================= */
bool buzzerState = false;
bool ledState = true;
bool buzzerManualOverride = false;
bool ledManualOverride = false;
bool isManualMode = false;  // true = manual, false = auto
int gasValue = 0;

// Timer cho manual override
unsigned long buzzerOverrideTime = 0;
unsigned long ledOverrideTime = 0;
unsigned long manualModeStartTime = 0;

/* ================= MQTT ================= */
WiFiClient espClient;
PubSubClient client(espClient);

/* ================= CALLBACK ================= */
void callback(char* topic, byte* payload, unsigned int length) {
  String topicStr = String(topic);
  String cmd = "";
  for (int i = 0; i < length; i++) cmd += (char)payload[i];
  cmd.toLowerCase();

  // Xử lý lệnh điều khiển
  if (topicStr == topic_control) {
    if (!isManualMode) {
      Serial.println("NODE2: Từ chối lệnh - Đang ở chế độ AUTO");
      return; // Bỏ qua lệnh nếu không ở chế độ manual
    }
    
    if (cmd == "buzzer_on") {
      buzzerManualOverride = true;
      buzzerState = true;
      buzzerOverrideTime = millis();
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("NODE2: BUZZER manual ON");
    }
    else if (cmd == "buzzer_off") {
      buzzerManualOverride = false;
      buzzerState = false;
      buzzerOverrideTime = millis();
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("NODE2: BUZZER manual OFF");
    }
    else if (cmd == "led_on") {
      ledManualOverride = true;
      ledState = true;
      ledOverrideTime = millis();
      digitalWrite(LED_PIN, HIGH);
      Serial.println("NODE2: LED manual ON");
    }
    else if (cmd == "led_off") {
      ledManualOverride = false;
      ledState = false;
      ledOverrideTime = millis();
      digitalWrite(LED_PIN, LOW);
      Serial.println("NODE2: LED manual OFF");
    }
  }
  // Xử lý chế độ từ web
  else if (topicStr == topic_mode) {
    if (cmd == "manual") {
      isManualMode = true;
      manualModeStartTime = millis(); // Reset timer
      Serial.println("NODE2: Chuyển sang chế độ MANUAL");
    }
    else if (cmd == "auto") {
      isManualMode = false;
      ledManualOverride = false;
      buzzerManualOverride = false;
      Serial.println("NODE2: Chuyển sang chế độ AUTO");
    }
  }
}

/* ================= CHECK MANUAL TIMEOUT ================= */
void checkManualTimeout() {
  unsigned long currentTime = millis();
  
  // Kiểm tra timeout cho buzzer (5 giây)
  if (buzzerManualOverride && (currentTime - buzzerOverrideTime >= 5000)) {
    buzzerManualOverride = false;
    Serial.println("NODE2: BUZZER manual override timeout");
  }
  
  // Kiểm tra timeout cho LED (5 giây)
  if (ledManualOverride && (currentTime - ledOverrideTime >= 5000)) {
    ledManualOverride = false;
    Serial.println("NODE2: LED manual override timeout");
  }
  
  // Kiểm tra timeout chế độ manual (10 phút)
  if (isManualMode && (currentTime - manualModeStartTime >= MANUAL_TIMEOUT)) {
    isManualMode = false;
    ledManualOverride = false;
    buzzerManualOverride = false;
    
    Serial.println("NODE2: Tự động chuyển về chế độ AUTO sau 10 phút");
    
    // Gửi thông báo mode lên MQTT
    client.publish(topic_mode, "auto", false);
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  Serial.println("NODE2: Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nNODE2: WiFi connected!");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.println("NODE2: Connecting to MQTT...");
  while (!client.connected()) {
    if (client.connect("NODE2_GAS_REALTIME")) {
      Serial.println("NODE2: MQTT connected!");
      client.subscribe(topic_control);
      client.subscribe(topic_mode); // Subscribe topic mode
      Serial.println("NODE2: Subscribed to control & mode topics");
    } else {
      Serial.print("NODE2: MQTT connection failed, rc=");
      Serial.print(client.state());
      delay(500);
    }
  }
}

/* ================= LOOP ================= */
void loop() {
  client.loop();

  static unsigned long lastGas = 0;
  static unsigned long lastSend = 0;

  // Kiểm tra timeout cho manual override
  checkManualTimeout();

  if (millis() - lastGas >= GAS_INTERVAL) {
    lastGas = millis();
    gasValue = analogRead(GAS_PIN);

    // Chỉ điều khiển tự động khi ở chế độ AUTO
    if (!isManualMode) {
      // Chỉ điều khiển buzzer nếu không có manual override
      if (!buzzerManualOverride) {
        buzzerState = (gasValue > GAS_THRESHOLD);
        digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
      }
      
      // Chỉ điều khiển LED nếu không có manual override
      if (!ledManualOverride) {
        ledState = (gasValue > GAS_THRESHOLD);
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
      }
    }
  }

  if (millis() - lastSend >= STATUS_INTERVAL) {
    lastSend = millis();
    String json = "{";
    json += "\"gas\":" + String(gasValue) + ",";
    json += "\"buzzer\":" + String(buzzerState ? "true" : "false") + ",";
    json += "\"led\":" + String(ledState ? "true" : "false") + ",";
    json += "\"buzzer_override\":" + String(buzzerManualOverride ? "true" : "false") + ",";
    json += "\"led_override\":" + String(ledManualOverride ? "true" : "false") + ",";
    json += "\"mode\":" + String(isManualMode ? "\"manual\"" : "\"auto\"");
    json += "}";
    client.publish(topic_status, json.c_str(), false);
  }

  yield();
}