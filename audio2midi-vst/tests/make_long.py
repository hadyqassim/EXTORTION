import numpy as np, soundfile as sf
rng = np.random.default_rng(7)
sr = 22050; dur = 100
audio = np.zeros(int(sr*dur), dtype=np.float32)
hz = lambda m: 440*2**((m-69)/12)
scale = [40,43,47,52,55,59,64,67,71,76]  # E minor-ish tones
t0 = 0.0
while t0 < dur-4:
    for m in rng.choice(scale, size=rng.integers(1,4), replace=False):
        d = rng.uniform(0.8, 3.0); n = int(d*sr); t = np.arange(n)/sr
        env = np.minimum(1, t*30)*np.exp(-t*0.5)
        tone = 0.2*env*(np.sin(2*np.pi*hz(m)*t)+0.3*np.sin(4*np.pi*hz(m)*t))
        i = int(t0*sr); audio[i:i+n] += tone[:max(0,len(audio)-i)][:n] if i+n<=len(audio) else tone[:len(audio)-i]
    t0 += rng.uniform(0.7, 2.0)
sf.write("test_long.wav", audio, sr)
