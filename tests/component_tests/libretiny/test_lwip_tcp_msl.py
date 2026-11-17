"""Every LibreTiny family frees closed TCP connections after about 1 s (TCP_MSL=250)."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    "config_file", ["bk72xx.yaml", "rtl8710b.yaml", "rtl8720c.yaml", "ln882x.yaml"]
)
def test_lwip_tcp_msl_is_250(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    generate_main(component_config_path(config_file))
    assert "TCP_MSL=250" in CORE.platformio_options["custom_options.lwip"]
