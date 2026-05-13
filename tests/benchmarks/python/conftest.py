"""Shared fixtures for the Python benchmark suite.

These benchmarks run under pytest-codspeed in CI. They are deliberately
kept out of the regular pytest test selection because they hit large
real components (full config validation against a complete YAML).
"""

from __future__ import annotations

from pathlib import Path
import sys

import pytest

from esphome.core import CORE

HERE = Path(__file__).parent

# Make sure ``esphome`` resolves to the in-tree package when the suite is
# run directly from a source checkout.
sys.path.insert(0, (HERE.parent.parent.parent).as_posix())


@pytest.fixture(autouse=True)
def reset_core_state() -> None:
    """Reset CORE before and after every benchmark.

    The fast path (``load_compiled_config``) and the slow path
    (``read_config``) both mutate CORE extensively. Without this fixture
    state leaks between benchmark cases and skews the numbers.
    """
    CORE.reset()
    yield
    CORE.reset()
