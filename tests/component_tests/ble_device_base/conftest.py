"""Local fixtures for the ble_device_base component tests."""

from collections.abc import Generator

import pytest

from esphome.core import CORE


@pytest.fixture(autouse=True)
def _fresh_core() -> Generator[None]:
    """Reset CORE before each test.

    The session-wide reset fixture only runs after component tests; a unit test
    that ran earlier on the same xdist worker can leave CORE populated, which
    breaks full-codegen runs (platform validation reads stale CORE.data).
    """
    CORE.reset()
    yield
