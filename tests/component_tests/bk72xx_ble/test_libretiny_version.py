"""bk72xx_ble refuses LibreTiny releases older than 1.14.0: their WiFi sleep arms the
Beken MCU sleep, which halts the device once the station stops while BLE runs."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.bk72xx_ble import check_libretiny_version
from esphome.config import read_config
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.mark.parametrize("version", ["1.14.0", "1.14.1", "1.15.0", "2.0.0"])
def test_new_enough_releases_pass(version: str) -> None:
    check_libretiny_version(cv.Version.parse(version))


@pytest.mark.parametrize("version", ["1.13.0", "1.12.2", "0.0.0"])
def test_older_releases_fail(version: str) -> None:
    with pytest.raises(cv.Invalid, match="1.14.0 or newer"):
        check_libretiny_version(cv.Version.parse(version))


def test_pinned_old_framework_fails_validation(
    component_config_path: Callable[[str], Path],
    capsys: pytest.CaptureFixture[str],
) -> None:
    CORE.config_path = component_config_path("test_libretiny_too_old.yaml")
    assert read_config({}) is None
    assert "1.14.0 or newer" in capsys.readouterr().out
