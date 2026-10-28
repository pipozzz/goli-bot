"""Render Goli's sounds (same formulas as src/sound.cpp) to docs/sounds/*.wav to listen on a computer."""
import math
import os
import struct
import wave

RATE = 16000
BELL = [(0.50, 0.35, 1.6), (1.00, 1.00, 1.1), (1.19, 0.45, 0.8), (1.50, 0.30, 0.6),
        (2.00, 0.55, 0.55), (2.52, 0.25, 0.35), (2.74, 0.20, 0.3), (3.76, 0.12, 0.2)]


def env(t, attack, decay):
    return (t / attack if t < attack else 1.0) * math.exp(-t / decay)


def ding(t, dur):
    return 0.32 * sum(a * env(t, 0.002, d) * math.sin(2 * math.pi * 1318.5 * r * t) for r, a, d in BELL)


def boop(t, dur):
    return 0.8 * env(t, 0.004, 0.06) * math.sin(2 * math.pi * (420 + 900 * t / dur) * t)


def chirp(t, dur):
    f, tt = (1046.5, t) if t < 0.14 else (1568.0, t - 0.14)
    return 0.55 * env(tt, 0.004, 0.07) * math.sin(2 * math.pi * f * t)


def wobble(t, dur):
    f = 700 - 300 * t / dur + 90 * math.sin(2 * math.pi * 11 * t)
    return 0.45 * (1 - t / dur) * math.sin(2 * math.pi * f * t)


def yawn(t, dur):
    f = 330 - 140 * t / dur
    return 0.5 * math.sin(math.pi * t / dur) * (math.sin(2 * math.pi * f * t) + 0.3 * math.sin(4 * math.pi * f * t))


SOUNDS = {"ding": (ding, 1.8), "boop": (boop, 0.16), "chirp": (chirp, 0.32), "wobble": (wobble, 0.7), "yawn": (yawn, 1.1)}

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs", "sounds")
os.makedirs(out, exist_ok=True)
for name, (fn, dur) in SOUNDS.items():
    with wave.open(os.path.join(out, f"{name}.wav"), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        n = int(dur * RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, fn(i / RATE, dur))) * 30000)) for i in range(n)))
    print("wrote", name)
