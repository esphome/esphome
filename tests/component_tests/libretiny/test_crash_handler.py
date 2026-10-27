"""Only BK7238 and BK7231N, the BK chips with an ITCM linker region, get the crash handler."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.libretiny import BK_TRAP_HANDLERS
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "enabled"),
    [
        ("bk7238.yaml", True),
        ("bk72xx.yaml", True),  # BK7231N
        ("bk7231t.yaml", False),
        ("ln882x.yaml", False),
    ],
)
def test_crash_handler_family_gate(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
    enabled: bool,
) -> None:
    generate_main(component_config_path(config))
    defines = {define.name for define in CORE.defines}
    assert ("USE_LIBRETINY_CRASH_HANDLER" in defines) is enabled
    assert ("USE_CRASH_HANDLER" in defines) is enabled
    for trap in BK_TRAP_HANDLERS:
        assert (f"-Wl,--wrap={trap}" in CORE.build_flags) is enabled
    assert (
        "pre:patch_noinit.py" in CORE.platformio_options["extra_scripts"]
    ) is enabled
