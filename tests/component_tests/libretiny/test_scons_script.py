"""The SCons 4.11 script is only loaded for LibreTiny releases before 1.14.0, which
fixed the library queue it patches around (libretiny-eu/libretiny#422)."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_current_release_skips_the_script(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("bk72xx.yaml"))
    assert "pre:scons_dont_inherit.py" not in CORE.platformio_options["extra_scripts"]


def test_pinned_old_release_loads_the_script(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("bk72xx_lt113.yaml"))
    assert "pre:scons_dont_inherit.py" in CORE.platformio_options["extra_scripts"]
