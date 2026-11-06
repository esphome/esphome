"""Shared helpers: 16-bit WAV loading and the normalised 10 ms envelope used to find the wake phrase."""

import functools
import os
import pathlib
import wave

import numpy as np

# The wake phrase as a 16 kHz mono WAV (see the recipe); found by envelope cross-correlation in the device's audio.
WAKE_REF = pathlib.Path(os.environ.get("WAKE_REF", "wake16k.wav"))


def load(p: pathlib.Path | str) -> np.ndarray:
    with wave.open(str(p)) as w:
        return np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(
            np.float32
        )


def env(x: np.ndarray, hop: int = 160) -> np.ndarray:
    n = len(x) // hop
    return np.sqrt((x[: n * hop].reshape(n, hop) ** 2).mean(axis=1) + 1e-9)


def norm(e: np.ndarray) -> np.ndarray:
    return (e - e.mean()) / (e.std() + 1e-9)


@functools.cache
def _ref() -> np.ndarray:
    return norm(env(load(WAKE_REF)))


def find_phrase(x: np.ndarray) -> tuple[int, float, int]:
    """Return (start in 10 ms steps, correlation, reference length in steps) of the best match."""
    ref = _ref()
    c = np.correlate(norm(env(x)), ref, mode="valid") / len(ref)
    k = int(c.argmax())
    return k, float(c[k]), len(ref)
