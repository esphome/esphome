"""automation.cpp (CronTrigger) is only compiled when an on_time automation
exists, so the define must follow it; on_time_sync needs no class."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    ("fixture", "emits"),
    [
        ("no_triggers.yaml", False),
        ("on_time.yaml", True),
        ("on_time_sync.yaml", False),
    ],
)
def test_triggers_define_follows_automations(
    fixture: str,
    emits: bool,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path(fixture))
    defines = {define.name for define in CORE.defines}
    assert ("USE_TIME_TRIGGERS" in defines) is emits
