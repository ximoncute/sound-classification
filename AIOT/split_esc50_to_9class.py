import os
import csv
import shutil

ESC_PATH = "ESC-50"
AUDIO_PATH = os.path.join(ESC_PATH, "audio")
CSV_PATH = os.path.join(ESC_PATH, "meta", "esc50.csv")

OUT_PATH = "my_9_classes"

MAP = {
    "door_knock": "1_tieng_go",
    "knocking": "1_tieng_go",

    "footsteps": "2_buoc_chan",

    "clapping": "3_vo_tay",

    "speech": "4_noi_binh_thuong",
    "coughing": "4_noi_binh_thuong",
    "laughing": "4_noi_binh_thuong",
    "sneezing": "4_noi_binh_thuong",

    "screaming": "5_tieng_het",
    "crying_baby": "5_tieng_het",

    "glass_breaking": "6_va_dap",
    "crash": "6_va_dap",

    "dog": "7_dong_vat",
    "cat": "7_dong_vat",
    "rooster": "7_dong_vat",

    "rain": "8_on_nen",
    "wind": "8_on_nen",
    "vacuum_cleaner": "8_on_nen",
    "engine": "8_on_nen",
    "clock_tick": "8_on_nen"
}

os.makedirs(OUT_PATH, exist_ok=True)

count = {}

with open(CSV_PATH, newline='', encoding="utf-8") as f:
    reader = csv.DictReader(f)
    for row in reader:
        cat = row["category"]
        if cat not in MAP:
            continue

        src = os.path.join(AUDIO_PATH, row["filename"])
        dst_dir = os.path.join(OUT_PATH, MAP[cat])
        os.makedirs(dst_dir, exist_ok=True)

        dst = os.path.join(dst_dir, row["filename"])
        shutil.copy(src, dst)

        count[MAP[cat]] = count.get(MAP[cat], 0) + 1

print("DONE. File count:")
for k, v in count.items():
    print(f"{k}: {v}")
