"""Tests for wifi.force_power_save_off(), the hook platforms use to keep the
station out of power save."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components import wifi
from esphome.core import CORE, EsphomeError


def test_reasons_accumulate_without_duplicates() -> None:
    """Every caller's reason is kept once; a repeated reason is not duplicated."""
    wifi.force_power_save_off("first")
    wifi.force_power_save_off("first")
    wifi.force_power_save_off("second")

    assert CORE.data[wifi.POWER_SAVE_OFF_REASONS_KEY] == ["first", "second"]


def test_forced_off_skips_the_setter_and_warns(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    """With a reason recorded, power_save_mode is reported and not applied."""
    wifi.force_power_save_off("the platform cannot sleep")

    main_cpp = generate_main(component_config_path("custom.yaml"))

    assert "set_power_save_mode(" not in main_cpp
    assert (
        "power_save_mode LIGHT is not applied: the platform cannot sleep" in caplog.text
    )


def test_call_after_wifi_codegen_raises(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Once wifi has generated its code the hook cannot take effect any more."""
    generate_main(component_config_path("custom.yaml"))

    with pytest.raises(EsphomeError, match="before wifi generates its code"):
        wifi.force_power_save_off("too late")
