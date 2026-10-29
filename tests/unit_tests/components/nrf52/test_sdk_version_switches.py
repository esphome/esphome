"""Behavior chosen by the sdk-nrf version."""

import pytest

from esphome.components import nrf52
from esphome.components.nrf52 import framework
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE


@pytest.mark.parametrize(
    ("version", "expected"),
    [
        ("2.9.2", "adafruit_itsybitsy_nrf52840"),
        ("3.0.0", "adafruit_itsybitsy"),
        ("3.4.1", "adafruit_itsybitsy"),
    ],
)
def test_old_board_name_is_translated_from_sdk_3_0(version: str, expected: str) -> None:
    assert (
        nrf52._zephyr_board("adafruit_itsybitsy_nrf52840", cv.Version.parse(version))
        == expected
    )


def test_other_board_names_are_kept() -> None:
    assert nrf52._zephyr_board("xiao_ble", cv.Version.parse("3.4.1")) == "xiao_ble"


@pytest.mark.parametrize(
    ("version", "zigbee", "expected"),
    [
        ("2.9.2", True, False),
        ("3.0.0", True, True),
        ("3.4.1", True, True),
        ("3.4.1", False, False),
    ],
)
def test_ncs_zigbee_workspace_from_sdk_3_0(
    version: str, zigbee: bool, expected: bool
) -> None:
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: cv.Version.parse(version)}

    assert framework._uses_ncs_zigbee(zigbee) is expected
    assert framework._get_framework_cache_key(f"v{version}", zigbee) == (
        f"v{version}-zigbee" if expected else f"v{version}"
    )


def test_default_sdk_has_a_paired_ncs_zigbee_version() -> None:
    assert f"v{nrf52.RECOMMENDED_SDK_NRF_VERSION}" in framework.NCS_ZIGBEE_VERSIONS
