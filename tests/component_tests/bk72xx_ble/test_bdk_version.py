"""bk72xx_ble refuses a beken-bdk override outside 3.0.78 and later 3.0.x releases:
an older BDK can crash at boot or erase the bootloader with BLE compiled in, and
LibreTiny only builds the 3.0 line."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.bk72xx_ble import check_bdk_version
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.mark.parametrize("value", [None, "3.0.78", "v3.0.78", "3.0.80"])
def test_new_enough_versions_pass(value: str | None) -> None:
    check_bdk_version(value)


@pytest.mark.parametrize(
    "value",
    [
        "3.0.33",
        "3.0.77",
        "3.1.0",
        "4.0.0",
        "2021.06.07",
        "main",
        "https://github.com/x/bdk#3.0.78",
    ],
)
def test_other_versions_fail(value: str) -> None:
    with pytest.raises(cv.Invalid, match="3.0.78 or a newer 3.0.x"):
        check_bdk_version(value)


def test_old_override_fails_validation(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("test_bdk_too_old.yaml")
    assert read_config({}) is None
    assert "3.0.78 or a newer 3.0.x" in capsys.readouterr().out


def test_newer_override_builds(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("test_bdk_newer.yaml"))
    assert "bk72xx_ble::BK72xxBLE" in main_cpp
    assert CORE.platformio_options.get("custom_versions.beken-bdk") == "3.0.80"
