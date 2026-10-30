"""The crash handler is built for BK7238, BK7231N (both with an ITCM linker region), RTL8720C and LN882H."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.libretiny import BK_TRAP_HANDLERS
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "enabled", "bk_hooks"),
    [
        ("bk7238.yaml", True, True),
        ("bk72xx.yaml", True, True),  # BK7231N
        ("bk7231t.yaml", False, False),
        ("rtl8720c.yaml", True, False),  # installed at run time
        ("rtl8710b.yaml", False, False),
        ("ln882x.yaml", True, False),  # installed at run time
    ],
)
def test_crash_handler_family_gate(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
    enabled: bool,
    bk_hooks: bool,
) -> None:
    generate_main(component_config_path(config))
    defines = {define.name for define in CORE.defines}
    assert ("USE_LIBRETINY_CRASH_HANDLER" in defines) is enabled
    assert ("USE_CRASH_HANDLER" in defines) is enabled
    for trap in BK_TRAP_HANDLERS:
        assert (f"-Wl,--wrap={trap}" in CORE.build_flags) is bk_hooks
    assert (
        "pre:patch_noinit.py" in CORE.platformio_options["extra_scripts"]
    ) is bk_hooks
