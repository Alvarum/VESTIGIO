#!/usr/bin/env python3
"""Generate original, deterministic VESTIGIO demo audio (no samples or dependencies)."""

from __future__ import annotations

import math
from pathlib import Path
import struct
import wave


SAMPLE_RATE = 22_050
ROOT = Path(__file__).resolve().parents[1] / "assets" / "demo" / "audio"


def noise_values(count: int, seed: int) -> list[float]:
    """Small fixed integer generator; independent of Python's random implementation."""
    values = []
    state = seed
    for _ in range(count):
        state ^= (state << 13) & 0xFFFFFFFF
        state ^= state >> 17
        state ^= (state << 5) & 0xFFFFFFFF
        state &= 0xFFFFFFFF
        values.append((state / 0xFFFFFFFF) * 2.0 - 1.0)
    return values


def cyclic_noise(knots: list[float], phase: float) -> float:
    point = phase * len(knots)
    first = int(point) % len(knots)
    blend = point - int(point)
    blend = blend * blend * (3.0 - 2.0 * blend)
    return knots[first] * (1.0 - blend) + knots[(first + 1) % len(knots)] * blend


def ambience() -> list[float]:
    duration = 4.0
    count = int(SAMPLE_RATE * duration)
    low_noise = noise_values(96, 0x56455354)
    high_noise = noise_values(800, 0x47494F21)
    samples = []
    for index in range(count):
        t = index / SAMPLE_RATE
        phase = index / count
        breathing = 0.82 + 0.18 * math.sin(2.0 * math.pi * 0.5 * t)
        drone = (0.095 * math.sin(2.0 * math.pi * 55.0 * t)
                 + 0.055 * math.sin(2.0 * math.pi * 82.5 * t + 0.6)
                 + 0.035 * math.sin(2.0 * math.pi * 41.25 * t + 1.4))
        distant_tone = (0.014 * math.sin(2.0 * math.pi * 330.0 * t + 0.3)
                        + 0.009 * math.sin(2.0 * math.pi * 440.0 * t + 2.1))
        air = (0.018 * cyclic_noise(low_noise, phase)
               + 0.006 * cyclic_noise(high_noise, phase))
        samples.append(breathing * drone + distant_tone + air)
    return samples


def door_move() -> list[float]:
    duration = 0.7
    count = round(SAMPLE_RATE * duration)
    grit = noise_values(count, 0x444F4F52)
    samples = []
    for index in range(count):
        t = index / SAMPLE_RATE
        progress = index / (count - 1)
        envelope = math.sin(math.pi * progress) ** 0.65
        phase = 2.0 * math.pi * (130.0 * t - 55.0 * t * t / (2.0 * duration))
        creak = (0.17 * math.sin(phase)
                 + 0.055 * math.sin(2.03 * phase + 0.2)
                 + 0.025 * math.sin(3.71 * phase))
        scrape = 0.037 * grit[index] * (1.0 - progress)
        latch = (0.11 * math.exp(-65.0 * t) * math.sin(2.0 * math.pi * 190.0 * t)
                 + 0.08 * math.exp(-75.0 * (duration - t))
                 * math.sin(2.0 * math.pi * 155.0 * t))
        samples.append(envelope * (creak + scrape + latch))
    return samples


def write_pcm16(path: Path, samples: list[float]) -> None:
    pcm = b"".join(struct.pack("<h", round(max(-1.0, min(1.0, value)) * 32767))
                   for value in samples)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(pcm)


def main() -> None:
    ROOT.mkdir(parents=True, exist_ok=True)
    write_pcm16(ROOT / "atrium-ambience.wav", ambience())
    write_pcm16(ROOT / "door-move.wav", door_move())


if __name__ == "__main__":
    main()
