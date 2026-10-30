"""Exercise VL53L1X code generation."""

from collections.abc import Callable
from pathlib import Path
import re

import pytest

SETTERS = (
    "set_distance_mode",
    "set_timing_budget",
    "set_signal_rate_limit",
    "set_roi",
    "set_roi_center",
    "set_offset",
)


def _setter_calls(source: str, sensor_id: str) -> list[str]:
    """Return the option setter calls emitted for one sensor."""
    pattern = rf"\b{sensor_id}->((?:{'|'.join(SETTERS)})\(.*\));"
    return re.findall(pattern, source)


def test_defaults_emit_no_setters(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    source = generate_main(component_config_path("defaults.yaml"))
    assert "vl_defaults->set_update_interval(" in source
    assert _setter_calls(source, "vl_defaults") == []


@pytest.mark.parametrize(
    ("sensor_id", "expected"),
    [
        ("vl_mode", ["set_distance_mode(vl53l1x::DISTANCE_MODE_MEDIUM)"]),
        ("vl_budget", ["set_timing_budget(100)"]),
        ("vl_rate", ["set_signal_rate_limit(256)"]),
        # 0.1 MCPS is 12.8 in 9.7 fixed point; the register value truncates
        ("vl_rate_fraction", ["set_signal_rate_limit(12)"]),
        # 1.005 MCPS truncates to 128, the register value the default already holds
        ("vl_rate_same_register", []),
        # 1.00781249 rounds to float32 1.0078125 (129/128), as the old float setter did
        ("vl_rate_float32", ["set_signal_rate_limit(129)"]),
        ("vl_width", ["set_roi(8, 16)"]),
        ("vl_height", ["set_roi(16, 8)"]),
        ("vl_center", ["set_roi(8, 8)", "set_roi_center(167)"]),
        ("vl_offset", ["set_offset(-5)"]),
    ],
)
def test_non_defaults_emit_only_their_setter(
    generate_main: Callable[[Path], str],
    component_config_path: Callable[[str], Path],
    sensor_id: str,
    expected: list[str],
) -> None:
    source = generate_main(component_config_path("custom.yaml"))
    assert f"{sensor_id}->set_update_interval(" in source
    assert _setter_calls(source, sensor_id) == expected
