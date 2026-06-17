"""Shared fixtures for the Python benchmark suite."""

from __future__ import annotations

from collections.abc import Generator

import pytest

from esphome.core import CORE


@pytest.fixture(autouse=True)
def reset_core_state() -> Generator[None]:
    """Reset CORE before and after every benchmark.

    Per-iteration setups inside benchmarks reset CORE for the loop body;
    this fixture handles the test-level boundary so stale state from
    fixture priming doesn't leak across benchmarks.
    """
    CORE.reset()
    yield
    CORE.reset()
