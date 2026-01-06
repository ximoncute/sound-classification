import os
import numpy as np
import librosa
import tensorflow as tf
from sklearn.model_selection import train_test_split
from sklearn.utils.class_weight import compute_class_weight
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import (
    Conv2D, MaxPooling2D, Flatten,
    Dense, Dropout, BatchNormalization
)
from tensorflow.keras.utils import to_categorical

# ======================
# CONFIG (PHẢI GIỐNG SERVER)
# ======================
DATASET_DIR = "my_9_classes"
SAMPLE_RATE = 8000          
N_MELS = 64
FIXED_FRAMES = 64
EPOCHS = 30
BATCH_SIZE = 16

CLASSES = sorted(os.listdir(DATASET_DIR))
NUM_CLASSES = len(CLASSES)

print("Train classes:", CLASSES)

# ======================
# FEATURE EXTRACTION
# ======================
def extract_feature(file_path):
    audio, _ = librosa.load(file_path, sr=SAMPLE_RATE, mono=True)

    # chuẩn hóa [-1,1]
    audio = audio / np.max(np.abs(audio) + 1e-6)

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

    return mel_db.astype(np.float32)

# ======================
# LOAD DATASET
# ======================
X, y = [], []

for label, cls in enumerate(CLASSES):
    folder = os.path.join(DATASET_DIR, cls)
    for f in os.listdir(folder):
        if f.endswith(".wav"):
            feat = extract_feature(os.path.join(folder, f))
            X.append(feat)
            y.append(label)

X = np.array(X)[..., np.newaxis]
y = np.array(y)

print("Dataset:", X.shape, y.shape)

# ======================
# CLASS WEIGHT (CỰC KỲ QUAN TRỌNG)
# ======================
class_weights = compute_class_weight(
    class_weight="balanced",
    classes=np.unique(y),
    y=y
)
class_weights = dict(enumerate(class_weights))
print("Class weight:", class_weights)

y_cat = to_categorical(y, NUM_CLASSES)

# ======================
# SPLIT
# ======================
X_train, X_test, y_train, y_test = train_test_split(
    X, y_cat,
    test_size=0.2,
    random_state=42,
    stratify=y
)

# ======================
# MODEL (CHỐNG OVERFIT + NHẬN DIỆN NGẮN)
# ======================
model = Sequential([
    Conv2D(16, (3,3), padding="same", activation="relu",
           input_shape=(64,64,1)),
    BatchNormalization(),
    MaxPooling2D((2,2)),

    Conv2D(32, (3,3), padding="same", activation="relu"),
    BatchNormalization(),
    MaxPooling2D((2,2)),

    Conv2D(64, (3,3), padding="same", activation="relu"),
    BatchNormalization(),
    MaxPooling2D((2,2)),

    Flatten(),
    Dense(64, activation="relu"),
    Dropout(0.4),
    Dense(NUM_CLASSES, activation="softmax")
])

model.compile(
    optimizer=tf.keras.optimizers.Adam(learning_rate=0.0005),
    loss="categorical_crossentropy",
    metrics=["accuracy"]
)

model.summary()

# ======================
# TRAIN
# ======================
history = model.fit(
    X_train, y_train,
    epochs=EPOCHS,
    batch_size=BATCH_SIZE,
    validation_data=(X_test, y_test),
    class_weight=class_weights
)

# ======================
# EVALUATE
# ======================
pred = np.argmax(model.predict(X_test), axis=1)
true = np.argmax(y_test, axis=1)
print("Pred unique:", np.unique(pred, return_counts=True))

# ======================
# SAVE
# ======================
model.save("sound_model.keras")

converter = tf.lite.TFLiteConverter.from_keras_model(model)
converter.optimizations = [tf.lite.Optimize.DEFAULT]
tflite_model = converter.convert()

with open("sound_model.tflite", "wb") as f:
    f.write(tflite_model)

print("✅ TRAIN DONE – MODEL OK")
