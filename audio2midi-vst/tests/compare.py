import sys, subprocess, numpy as np, librosa, warnings, os
warnings.filterwarnings("ignore")
import logging; logging.disable(logging.CRITICAL)
from basic_pitch.inference import predict
MODEL = "assets/nmp.onnx"
wav, onset, frame, min_ms = sys.argv[1], float(sys.argv[2]), float(sys.argv[3]), float(sys.argv[4])

y, _ = librosa.load(wav, sr=22050, mono=True)
y.astype(np.float32).tofile("tmp.f32")
min_frames = int(np.round(min_ms/1000*(22050/256)))

_, _, ref = predict(wav, MODEL, onset_threshold=onset, frame_threshold=frame, minimum_note_length=min_ms)
ref = sorted([(int(n[2]), float(n[0]), float(n[1]), float(n[3])) for n in ref])

out = subprocess.run(["./harness", MODEL, "tmp.f32", str(onset), str(frame), str(min_frames)], capture_output=True, text=True)
lines = out.stdout.strip().split("\n")
print(lines[0])
cpp = sorted([(int(l.split()[0]), float(l.split()[1]), float(l.split()[2]), float(l.split()[3])) for l in lines[1:]])
print("python notes:", len(ref), " c++ notes:", len(cpp))

# match each python note to a c++ note with same pitch and close start
used = set(); worst_s = worst_e = worst_a = 0; matched = 0
for p in ref:
    best = None
    for j, c in enumerate(cpp):
        if j in used or c[0] != p[0]: continue
        d = abs(c[1]-p[1])
        if d < 0.05 and (best is None or d < best[0]): best = (d, j)
    if best:
        used.add(best[1]); c = cpp[best[1]]; matched += 1
        worst_s = max(worst_s, abs(c[1]-p[1])); worst_e = max(worst_e, abs(c[2]-p[2])); worst_a = max(worst_a, abs(c[3]-p[3]))
print(f"matched {matched}/{len(ref)}  max start diff {worst_s*1000:.2f} ms  max end diff {worst_e*1000:.2f} ms  max amp diff {worst_a:.4f}")
print("unmatched c++ notes:", len(cpp)-len(used))
