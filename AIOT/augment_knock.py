import librosa
import soundfile as sf
import numpy as np
import os
import random

DIR = "my_9_classes/1_tieng_go"

files = os.listdir(DIR)

for f in files:
    y, sr = librosa.load(os.path.join(DIR, f), sr=16000)

    # pitch shift nhẹ
    y1 = librosa.effects.pitch_shift(y, sr, n_steps=random.uniform(-0.5, 0.5))

    # time stretch rất nhẹ
    y2 = librosa.effects.time_stretch(y, random.uniform(0.95, 1.05))

    sf.write(os.path.join(DIR, "ps_"+f), y1, sr)
    sf.write(os.path.join(DIR, "ts_"+f), y2, sr)
