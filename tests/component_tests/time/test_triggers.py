"""The clock's once-a-second on_time check is only compiled when an on_time automation
exists, so the define must follow it; on_time_sync needs no check."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

from ..helpers import get_define_value


@pytest.mark.parametrize(
    ("fixture", "emits", "starts", "slots"),
    [
        ("no_triggers.yaml", False, 0, None),
        ("on_time.yaml", True, 1, "1"),
        ("on_time_sync.yaml", False, 0, None),
        ("on_time_fields.yaml", True, 1, "8"),
        # the callback list is per clock: its size is the largest count on one clock, not the total
        ("on_time_two_clocks.yaml", True, 2, "2"),
    ],
)
def test_triggers_define_follows_automations(
    fixture: str,
    emits: bool,
    starts: int,
    slots: str | None,
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path(fixture))
    defines = {define.name for define in CORE.defines}
    assert ("USE_TIME_TRIGGERS" in defines) is emits
    # one interval per clock with on_time
    assert main_cpp.count("start_on_time_checks();") == starts
    assert get_define_value("TIME_ON_TIME_COUNT") == slots
