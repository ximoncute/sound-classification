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

/* ================= CONFIG ================= */
#define GAS_THRESHOLD 400
#define GAS_INTERVAL  50
#define STATUS_INTERVAL 80

/* ================= STATE ================= */
bool buzzerState = false;
bool ledState = true;
bool buzzerManualOverride = false;
bool ledManualOverride = false;
int gasValue = 0;

/* ================= MQTT ================= */
WiFiClient espClient;
PubSubClient client(espClient);

/* ================= CALLBACK ================= */
void callback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != topic_control) return;

  String cmd = "";
  for (int i = 0; i < length; i++) cmd += (char)payload[i];
  cmd.toLowerCase();

  if (cmd == "buzzer_on") {
    buzzerManualOverride = true;
    buzzerState = true;
    digitalWrite(BUZZER_PIN, HIGH);
  }
  else if (cmd == "buzzer_off") {
    buzzerManualOverride = false;
    buzzerState = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
  else if (cmd == "led_on") {
    ledManualOverride = true;
    ledState = true;
    digitalWrite(LED_PIN, HIGH);
  }
  else if (cmd == "led_off") {
    ledManualOverride = false;
    ledState = false;
    digitalWrite(LED_PIN, LOW);
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  while (!client.connected()) {
    client.connect("NODE2_GAS_REALTIME");
    delay(500);
  }
  client.subscribe(topic_control);
}

/* ================= LOOP ================= */
void loop() {
  client.loop();

  static unsigned long lastGas = 0;
  static unsigned long lastSend = 0;

  if (millis() - lastGas >= GAS_INTERVAL) {
    lastGas = millis();
    gasValue = analogRead(GAS_PIN);

    if (!buzzerManualOverride) {
      buzzerState = (gasValue > GAS_THRESHOLD);
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    }
  }

  if (millis() - lastSend >= STATUS_INTERVAL) {
    lastSend = millis();
    String json = "{";
    json += "\"gas\":" + String(gasValue) + ",";
    json += "\"buzzer\":" + String(buzzerState ? "true" : "false") + ",";
    json += "\"led\":" + String(ledState ? "true" : "false");
    json += "}";
    client.publish(topic_status, json.c_str(), false);
  }

  yield();
}
