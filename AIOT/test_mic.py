import sounddevice as sd
import numpy as np
import librosa
import tensorflow as tf
from scipy.io.wavfile import write

# ======================
# CONFIG
# ======================
MODEL_PATH = "sound_model.h5"
SAMPLE_RATE = 8000
DURATION = 3        # seconds
N_MELS = 64
FIXED_FRAMES = 64

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

# ======================
# RECORD AUDIO
# ======================
print("Recording... Speak or make sound!")
audio = sd.rec(int(DURATION * SAMPLE_RATE),
                samplerate=SAMPLE_RATE,
                channels=1,
                dtype='float32')
sd.wait()
print("Recording finished")

audio = audio.flatten()
write("test.wav", SAMPLE_RATE, audio)

# ======================
# FEATURE EXTRACTION
# ======================
mel = librosa.feature.melspectrogram(
    y=audio,
    sr=SAMPLE_RATE,
    n_mels=N_MELS,
    n_fft=1024,
    hop_length=256
)

mel_db = librosa.power_to_db(mel, ref=np.max)

if mel_db.shape[1] < FIXED_FRAMES:
    pad = FIXED_FRAMES - mel_db.shape[1]
    mel_db = np.pad(mel_db, ((0, 0), (0, pad)))
else:
    mel_db = mel_db[:, :FIXED_FRAMES]

X = mel_db[np.newaxis, ..., np.newaxis]

# ======================
# LOAD MODEL & PREDICT
# ======================
model = tf.keras.models.load_model(MODEL_PATH)
pred = model.predict(X)[0]

idx = np.argmax(pred)
confidence = pred[idx] * 100

# ======================
# RESULT
# ======================
print("\n Prediction Result")
print("---------------------")
print(f" Class      : {CLASSES[idx]}")
print(f" Confidence : {confidence:.2f} %")

print("\n All class probabilities:")
for i, p in enumerate(pred):
    print(f"{CLASSES[i]:20s}: {p*100:.2f}%")
