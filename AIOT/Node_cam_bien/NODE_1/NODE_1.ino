#include <ESP8266WiFi.h>
#include <PubSubClient.h>

/* ================= PIN ================= */
#define MIC_PIN     A0
#define BUZZER_PIN  5
#define LED_PIN     2
#define TRIG_PIN    14
#define ECHO_PIN    12

/* ================= WIFI + MQTT ================= */
const char* ssid = "Hieu Cute";
const char* password = "ximon123";
const char* mqtt_server = "broker.emqx.io";

const char* topic_status  = "node1/status";
const char* topic_control = "node1/control";
const char* topic_audio   = "audio/chunk1";

/* ================= CONFIG ================= */
#define DISTANCE_THRESHOLD 50
#define BUZZER_THRESHOLD   300

#define DISTANCE_INTERVAL  40
#define AUDIO_INTERVAL     5
#define STATUS_INTERVAL    80

#define DIST_MIN_CM  2
#define DIST_MAX_CM  300
#define DIST_SAMPLES 5

/* ===== AUDIO STREAM FOR AI ===== */
#define AUDIO_SAMPLE_RATE 8000
#define AUDIO_CHUNK       256

uint16_t audioBuffer[AUDIO_CHUNK];
uint16_t audioIndex = 0;
unsigned long lastAudioMicros = 0;

/* ================= STATE ================= */
bool ledState = false;
bool buzzerState = false;
bool ledManualOverride = false;
bool buzzerManualOverride = false;

float currentDistance = -1;
int audioPeak = 0;

/* ================= MQTT ================= */
WiFiClient espClient;
PubSubClient client(espClient);

/* ================= DISTANCE ================= */
float singleDistanceRead() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  if (duration == 0) return -1;

  float d = duration * 0.0343f / 2.0f;
  if (d < DIST_MIN_CM || d > DIST_MAX_CM) return -1;
  return d;
}

float readDistanceStable() {
  float samples[DIST_SAMPLES];
  int valid = 0;

  for (int i = 0; i < DIST_SAMPLES; i++) {
    float d = singleDistanceRead();
    if (d > 0) samples[valid++] = d;
    delayMicroseconds(600);
  }

  if (valid == 0) return -1;

  for (int i = 0; i < valid - 1; i++)
    for (int j = i + 1; j < valid; j++)
      if (samples[j] < samples[i]) {
        float t = samples[i];
        samples[i] = samples[j];
        samples[j] = t;
      }

  return samples[valid / 2];
}

void processDistance() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < DISTANCE_INTERVAL) return;
  lastRead = millis();

  float d = readDistanceStable();
  if (d > 0) {
    currentDistance = d;
    if (!ledManualOverride) {
      ledState = (d < DISTANCE_THRESHOLD);
      digitalWrite(LED_PIN, ledState ? HIGH : LOW);
    }
  }
}

/* ================= AUDIO PEAK ================= */
void processAudioPeak() {
  static unsigned long lastRead = 0;
  if (millis() - lastRead < AUDIO_INTERVAL) return;
  lastRead = millis();

  int v = abs(analogRead(MIC_PIN) - 512);
  audioPeak = max(audioPeak, v);

  if (!buzzerManualOverride) {
    buzzerState = (audioPeak > BUZZER_THRESHOLD);
    digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
  }
}

/* ================= AUDIO RAW STREAM ================= */
void streamAudioRaw() {
  unsigned long now = micros();
  if (now - lastAudioMicros < 1000000UL / AUDIO_SAMPLE_RATE) return;
  lastAudioMicros = now;

  uint16_t sample = analogRead(MIC_PIN); // 0–1023
  audioBuffer[audioIndex++] = sample;

  if (audioIndex >= AUDIO_CHUNK) {
    client.publish(
      topic_audio,
      (uint8_t*)audioBuffer,
      AUDIO_CHUNK * 2,
      false
    );
    audioIndex = 0;
  }
}

/* ================= STATUS ================= */
void publishStatus() {
  static unsigned long lastSend = 0;
  if (millis() - lastSend < STATUS_INTERVAL) return;
  lastSend = millis();

  String json = "{";
  json += "\"distance\":" + String(currentDistance, 1) + ",";
  json += "\"audio_peak\":" + String(audioPeak) + ",";
  json += "\"led\":" + String(ledState ? "true" : "false") + ",";
  json += "\"buzzer\":" + String(buzzerState ? "true" : "false");
  json += "}";

  client.publish(topic_status, json.c_str(), false);
  audioPeak = 0;
}

/* ================= CALLBACK ================= */
void callback(char* topic, byte* payload, unsigned int length) {
  if (String(topic) != topic_control) return;

  String cmd;
  for (int i = 0; i < length; i++) cmd += (char)payload[i];
  cmd.toLowerCase();

  if (cmd == "led_on") {
    ledManualOverride = true;
    ledState = true;
    digitalWrite(LED_PIN, HIGH);
  }
  else if (cmd == "led_off") {
    ledManualOverride = false;
    ledState = false;
    digitalWrite(LED_PIN, LOW);
  }
  else if (cmd == "buzzer_on") {
    buzzerManualOverride = true;
    buzzerState = true;
    digitalWrite(BUZZER_PIN, HIGH);
  }
  else if (cmd == "buzzer_off") {
    buzzerManualOverride = false;
    buzzerState = false;
    digitalWrite(BUZZER_PIN, LOW);
  }
}

/* ================= SETUP ================= */
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(300);

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  while (!client.connected()) {
    client.connect("NODE1_AI_READY");
    delay(300);
  }

  client.subscribe(topic_control);
}

/* ================= LOOP ================= */
void loop() {
  client.loop();

  processDistance();
  processAudioPeak();
  publishStatus();

  streamAudioRaw();   // <<< CỰC KỲ QUAN TRỌNG CHO AI

  yield();
}
