import librosa
import numpy as np
import soundfile as sf
import os

INPUT_WAV = "door_knock.wav"
OUTPUT_DIR = "my_9_classes/1_tieng_go"
os.makedirs(OUTPUT_DIR, exist_ok=True)

y, sr = librosa.load(INPUT_WAV, sr=16000)

# phát hiện xung gõ
onsets = librosa.onset.onset_detect(
    y=y,
    sr=sr,
    backtrack=True,
    delta=0.2,
    units="samples"
)

print("Detected knocks:", len(onsets))

WINDOW = int(0.6 * sr)  # mỗi tiếng gõ ~600ms

count = 0
for o in onsets:
    start = max(0, o - int(0.05 * sr))
    end = min(len(y), start + WINDOW)
    clip = y[start:end]

    # bỏ tiếng quá nhỏ
    if np.max(np.abs(clip)) < 0.02:
        continue

    sf.write(
        f"{OUTPUT_DIR}/knock_{count:03d}.wav",
        clip,
        sr
    )
    count += 1

print("Saved:", count, "samples")
