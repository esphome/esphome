"""Configuration file for socket component tests."""

import pytest

from esphome.core import CORE


@pytest.fixture(autouse=True)
def reset_core():
    """Reset CORE after each test."""
    yield
    CORE.reset()
