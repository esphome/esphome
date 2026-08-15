"""Fixtures for NVM component tests."""

from __future__ import annotations

from collections.abc import Generator
from pathlib import Path
import sys

import pytest

# Add package root to python path
here = Path(__file__).parent
package_root = here.parent.parent.parent
sys.path.insert(0, package_root.as_posix())

from esphome.core import CORE  # noqa: E402


@pytest.fixture(autouse=True)
def reset_core() -> Generator[None]:
    """Reset CORE state before and after each test."""
    CORE.reset()
    yield
    CORE.reset()
