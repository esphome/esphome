"""Platform-neutral helpers for ninja-driven native builds."""

from __future__ import annotations

import os
from pathlib import Path
import shutil

from esphome.core import EsphomeError


def find_ninja() -> Path:
    """Locate the ninja binary: PATH first, else the ninja PyPI wheel.

    The wheel is a requirements.txt dependency, so pip has already
    integrity-checked it; no download logic is needed here.
    """
    if binary := shutil.which("ninja"):
        return Path(binary)
    try:
        import ninja
    except ImportError:
        wheel_binary = None
    else:
        wheel_binary = Path(ninja.BIN_DIR) / (
            "ninja.exe" if os.name == "nt" else "ninja"
        )
    if wheel_binary is None or not wheel_binary.is_file():
        raise EsphomeError(
            "ninja not found on PATH or in the ninja package; reinstall the "
            "esphome Python environment"
        )
    return wheel_binary
