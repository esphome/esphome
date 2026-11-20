"""Tests for RP2040 build flags."""

from collections.abc import Callable
from pathlib import Path

from esphome.core import CORE


def test_rp2_skips_libc_tz_reload(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """strftime() must not link newlib's TZ parser and its scanf engine."""
    generate_main(component_config_path("minimal.yaml"))

    assert "-Wl,--wrap=_tzset_unlocked" in CORE.build_flags
