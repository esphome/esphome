"""The crash handler is built for BK7238 and BK7231N only: it wraps the SDK's
fault handlers and needs the ITCM linker region those chips have."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE

TRAPS = ("bk_trap_udef", "bk_trap_pabt", "bk_trap_dabt")


def _defines() -> set[str]:
    return {define.name for define in CORE.defines}


@pytest.mark.parametrize("config", ["bk7238.yaml", "bk72xx.yaml"])
def test_bk7238_and_bk7231n_get_the_crash_handler(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
) -> None:
    generate_main(component_config_path(config))
    assert {"USE_LIBRETINY_CRASH_HANDLER", "USE_CRASH_HANDLER"} <= _defines()
    for trap in TRAPS:
        assert f"-Wl,--wrap={trap}" in CORE.build_flags
        assert f"-Wl,--undefined=__wrap_{trap}" in CORE.build_flags
    assert "pre:patch_noinit.py" in CORE.platformio_options["extra_scripts"]
    assert "pre:patch_linker.py" not in CORE.platformio_options["extra_scripts"]


@pytest.mark.parametrize("config", ["bk7231t.yaml", "ln882x.yaml"])
def test_other_families_do_not(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config: str,
) -> None:
    generate_main(component_config_path(config))
    assert not {"USE_LIBRETINY_CRASH_HANDLER", "USE_CRASH_HANDLER"} & _defines()
    assert not any("bk_trap_" in flag for flag in CORE.build_flags)
    assert "pre:patch_noinit.py" not in CORE.platformio_options["extra_scripts"]
