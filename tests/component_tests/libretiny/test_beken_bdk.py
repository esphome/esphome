"""BK7231N and BK7231T build with Beken BDK 3.0.78 by default; a platformio_options
entry in the config still picks the version."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize("config_file", ["bdk_bk7231n.yaml", "bdk_bk7231t.yaml"])
def test_bk7231_builds_with_bdk_3_0_78(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    generate_main(component_config_path(config_file))
    assert CORE.platformio_options.get("custom_versions.beken-bdk") == "3.0.78"


def test_config_platformio_option_wins(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    generate_main(component_config_path("bdk_override.yaml"))
    assert CORE.platformio_options.get("custom_versions.beken-bdk") == "3.0.33"
