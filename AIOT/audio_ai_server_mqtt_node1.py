import numpy as np
import librosa
import struct
import time
import paho.mqtt.client as mqtt
import tensorflow as tf
from collections import deque

# ================= CONFIG =================
MQTT_BROKER = "broker.emqx.io"
MQTT_TOPIC  = "audio/chunk1"

SAMPLE_RATE = 8000
TARGET_SAMPLES = 8000        # 1 giây audio
AUDIO_CHUNK = 256            # ESP gửi 256 mẫu / lần

N_MELS = 64
FIXED_FRAMES = 64

ENERGY_THRESHOLD = 0.03      # LỌC IM LẶNG
CONF_THRESHOLD   = 0.6       # ĐỘ TIN CẬY TỐI THIỂU
ON_NEN_STRICT    = 0.9       # RIÊNG CHO ỒN NỀN

MODEL_PATH = "sound_model.tflite"

CLASSES = [
    "1_tieng_go",
    "2_buoc_chan",
    "3_vo_tay",
    "4_noi_binh_thuong",
    "5_tieng_het",
    "6_va_dap",
    "7_dong_vat",
    "8_on_nen",
    "9_im_lang"
]

# ================= LOAD MODEL =================
interpreter = tf.lite.Interpreter(model_path=MODEL_PATH)
interpreter.allocate_tensors()
input_details  = interpreter.get_input_details()
output_details = interpreter.get_output_details()

print("✅ AI model loaded")
print("Input shape:", input_details[0]['shape'])
print("🎧 AI SERVER RUNNING...\n")

# ================= AUDIO BUFFER =================
audio_buffer = deque(maxlen=TARGET_SAMPLES)

last_print_time = 0

# ================= PROCESS AUDIO =================
def process_audio(audio_np):
    global last_print_time

    # ADC 0–1023 → [-1,1]
    audio = (audio_np.astype(np.float32) - 512) / 512.0

    # ===== SILENCE GATE =====
    energy = np.mean(np.abs(audio))
    if energy < ENERGY_THRESHOLD:
        return

    # ===== MEL (GIỐNG TRAIN) =====
    mel = librosa.feature.melspectrogram(
        y=audio,
        sr=SAMPLE_RATE,
        n_mels=N_MELS,
        n_fft=1024,
        hop_length=256
    )
    mel_db = librosa.power_to_db(mel, ref=np.max)

    if mel_db.shape[1] < FIXED_FRAMES:
        mel_db = np.pad(
            mel_db,
            ((0, 0), (0, FIXED_FRAMES - mel_db.shape[1])),
            mode="constant"
        )
    else:
        mel_db = mel_db[:, :FIXED_FRAMES]

    mel_db = mel_db[np.newaxis, ..., np.newaxis].astype(np.float32)

    # ===== INFERENCE =====
    interpreter.set_tensor(input_details[0]['index'], mel_db)
    interpreter.invoke()
    pred = interpreter.get_tensor(output_details[0]['index'])[0]

    conf = float(np.max(pred))
    label = CLASSES[int(np.argmax(pred))]

    # ===== CONF FILTER =====
    if conf < CONF_THRESHOLD:
        label = "unknown"

    # ===== CHỐNG SPAM ỒN NỀN =====
    if label == "8_on_nen" and conf < ON_NEN_STRICT:
        return

    # ===== RATE LIMIT PRINT =====
    now = time.time()
    if now - last_print_time < 0.3:
        return
    last_print_time = now

    print(f"🧠 AI → {label} | conf={conf:.2f}")

# ================= MQTT CALLBACK =================
def on_message(client, userdata, msg):
    payload = msg.payload

    if len(payload) != AUDIO_CHUNK * 2:
        return

    samples = struct.unpack("<" + "H" * AUDIO_CHUNK, payload)

    audio_buffer.extend(samples)

    print(f"RX {len(samples)} samples | total={len(audio_buffer)}")

    if len(audio_buffer) >= TARGET_SAMPLES:
        audio_np = np.array(audio_buffer)
        audio_buffer.clear()

        print("🎯 PROCESS AUDIO")
        process_audio(audio_np)

# ================= MQTT SETUP =================
client = mqtt.Client()
client.on_message = on_message
client.connect(MQTT_BROKER, 1883, 60)
client.subscribe(MQTT_TOPIC)

print("📡 MQTT connected")
print("📡 Subscribed:", MQTT_TOPIC)

client.loop_forever()
