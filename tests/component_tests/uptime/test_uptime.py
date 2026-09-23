"""The timestamp uptime sensor source is only compiled when that type is used,
so the define must follow the configured sensor type rather than time: alone."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    ("fixture", "emits"),
    [
        ("seconds.yaml", False),
        ("timestamp.yaml", True),
    ],
)
def test_timestamp_define_follows_sensor_type(
    fixture: str,
    emits: bool,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path(fixture))
    defines = {define.name for define in CORE.defines}
    assert ("USE_UPTIME_TIMESTAMP" in defines) is emits
