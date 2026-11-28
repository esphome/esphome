"""Tests for the nRF52 _tzset_unlocked wrap."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_nrf52_skips_libc_tz_reload(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """strftime() must not link the C library's TZ parser and its scanf engine."""
    generate_main(component_config_path("nrf52840.yaml"))

    assert "-Wl,--wrap=_tzset_unlocked" in CORE.build_flags
