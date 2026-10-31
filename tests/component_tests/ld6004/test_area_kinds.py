"""The LD6004 sizes its dwell areas only when an entity uses them."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    ("config_file", "area_kinds"),
    [
        ("binary_sensor_only.yaml", "2"),
        ("detection_area_sensor.yaml", "2"),
        ("dwell_area_sensor.yaml", "3"),
    ],
)
def test_area_kinds(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
    area_kinds: str,
) -> None:
    generate_main(component_config_path(config_file))
    defines = {define.name: str(define.value) for define in CORE.defines}
    assert defines["LD600X_AREA_KINDS"] == area_kinds
    assert defines["LD600X_MAX_TARGETS"] == "3"
