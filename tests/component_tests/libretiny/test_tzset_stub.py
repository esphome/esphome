"""Tests for the LibreTiny _tzset_unlocked wrap."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.core import CORE


@pytest.mark.parametrize(
    "config_file", ["bk72xx.yaml", "rtl8710b.yaml", "rtl8720c.yaml", "ln882x.yaml"]
)
def test_libretiny_skips_libc_tz_reload(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    config_file: str,
) -> None:
    """strftime() must not link newlib's TZ parser and its scanf engine."""
    generate_main(component_config_path(config_file))

    assert "-Wl,--wrap=_tzset_unlocked" in CORE.build_flags
