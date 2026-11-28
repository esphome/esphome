"""Tests for the pulse_counter backend selection."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "hw", "basic"),
    [
        pytest.param("pulse_counter_hw.yaml", True, False, id="hw_only"),
        pytest.param("pulse_counter_basic.yaml", False, True, id="basic_only"),
        pytest.param("pulse_counter_mixed.yaml", True, True, id="mixed"),
    ],
)
def test_backend_defines(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
    hw: bool,
    basic: bool,
) -> None:
    """Only the backends the sensors use are compiled into the storage slot."""
    generate_main(component_config_path(config))

    defines = {d.name for d in CORE.defines}
    assert ("USE_PULSE_COUNTER_HW" in defines) is hw
    assert ("USE_PULSE_COUNTER_BASIC" in defines) is basic
