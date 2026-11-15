"""Генерация коротких WAV-эффектов для демо nehopio.

Запуск: python utils/nehopio/gen_sounds.py [папка_вывода]
Без аргумента пишет в assets/nehopio/sounds.
"""

import math
import random
import struct
import sys
import wave
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[2]
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else PROJECT / "assets" / "nehopio" / "sounds"
OUT.mkdir(parents=True, exist_ok=True)

RATE = 22050
random.seed(7)


def write(name, samples):
    clipped = [max(-32767, min(32767, int(value))) for value in samples]
    with wave.open(str(OUT / name), "wb") as handle:
        handle.setnchannels(1)
        handle.setsampwidth(2)
        handle.setframerate(RATE)
        handle.writeframes(struct.pack("<" + "h" * len(clipped), *clipped))


def square_sweep(start, end, duration, volume=0.35, decay=1.0):
    count = int(RATE * duration)
    phase = 0.0
    samples = []
    for index in range(count):
        position = index / max(1, count - 1)
        frequency = start + (end - start) * position
        phase += 2 * math.pi * frequency / RATE
        value = 1.0 if math.sin(phase) >= 0 else -1.0
        envelope = (1.0 - position) ** decay
        samples.append(32767 * volume * envelope * value)
    return samples


def noise_burst(duration, volume=0.3, decay=2.0):
    count = int(RATE * duration)
    samples = []
    for index in range(count):
        position = index / max(1, count - 1)
        envelope = (1.0 - position) ** decay
        samples.append(32767 * volume * envelope * random.uniform(-1.0, 1.0))
    return samples


def mix(*tracks):
    length = max(len(track) for track in tracks)
    result = [0.0] * length
    for track in tracks:
        for index, value in enumerate(track):
            result[index] += value
    return result


def sequence(notes):
    result = []
    for frequency, duration in notes:
        result.extend(square_sweep(frequency, frequency, duration, volume=0.3, decay=0.6))
    return result


write("jump.wav", square_sweep(320, 720, 0.13, volume=0.3, decay=1.2))
write("coin.wav", sequence([(988, 0.06), (1319, 0.11)]))
write("stomp.wav", mix(square_sweep(180, 110, 0.1, volume=0.35, decay=2.5), noise_burst(0.08, 0.15)))
write("break.wav", mix(noise_burst(0.16, 0.35, 2.2), square_sweep(220, 90, 0.12, volume=0.25)))
write("bump.wav", square_sweep(200, 150, 0.07, volume=0.3, decay=2.0))
write("powerup.wav", sequence([(392, 0.07), (523, 0.07), (659, 0.07), (784, 0.12)]))
write("win.wav", sequence([(523, 0.12), (659, 0.12), (784, 0.12), (1047, 0.22)]))
write("death.wav", square_sweep(440, 110, 0.55, volume=0.3, decay=0.8))

print("sounds:", sorted(path.name for path in OUT.iterdir()))
