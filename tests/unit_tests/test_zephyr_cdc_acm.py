"""Tests for CDC ACM port reservation in the Zephyr component."""

import pytest

from esphome.components.zephyr import (
    CdcAcmUser,
    _cdc_acm_add_ports,
    zephyr_claim_cdc_acm,
    zephyr_require_cdc_acm,
    zephyr_set_core_data,
)
from esphome.components.zephyr.const import KEY_BOOTLOADER, KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import (
    CONF_BOARD,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE


@pytest.fixture(autouse=True)
def clean_zephyr_data() -> None:
    CORE.data[KEY_ZEPHYR] = {"cdc_acm_users": {}}


def _users(id: int) -> list[str]:
    return [user.name for user in CORE.data[KEY_ZEPHYR]["cdc_acm_users"][id]]


def test_different_ports_can_be_claimed() -> None:
    zephyr_claim_cdc_acm(0, "logger", write_only=True)
    zephyr_claim_cdc_acm(1, "usb_cdc_acm")

    assert _users(0) == ["logger"]
    assert _users(1) == ["usb_cdc_acm"]


def test_same_user_can_claim_a_port_again() -> None:
    zephyr_claim_cdc_acm(0, "usb_cdc_acm")
    zephyr_claim_cdc_acm(0, "usb_cdc_acm")

    assert CORE.data[KEY_ZEPHYR]["cdc_acm_users"][0] == [
        CdcAcmUser("usb_cdc_acm", False, False)
    ]


LOGGER = ("logger", {"write_only": True})
RAW = ("usb_cdc_acm", {})
MCUMGR = ("zephyr_mcumgr", {"allow_write_only": True})


@pytest.mark.parametrize(
    ("first", "second", "ok"),
    [
        (LOGGER, RAW, False),
        (RAW, LOGGER, False),
        (LOGGER, MCUMGR, True),
        (MCUMGR, LOGGER, True),
    ],
)
def test_logger_sharing(
    first: tuple[str, dict[str, bool]], second: tuple[str, dict[str, bool]], ok: bool
) -> None:
    zephyr_claim_cdc_acm(0, first[0], **first[1])
    if not ok:
        with pytest.raises(cv.Invalid, match="is used by both"):
            zephyr_claim_cdc_acm(0, second[0], **second[1])
        return
    zephyr_claim_cdc_acm(0, second[0], **second[1])
    assert _users(0) == [first[0], second[0]]


def test_two_irq_users_are_rejected_even_if_they_allow_the_logger() -> None:
    zephyr_claim_cdc_acm(0, "zephyr_mcumgr", allow_write_only=True)

    with pytest.raises(cv.Invalid, match="'zephyr_mcumgr' and 'usb_cdc_acm'"):
        zephyr_claim_cdc_acm(0, "usb_cdc_acm")


def _overlay(*ports: int, required: bool = False) -> str:
    zephyr_set_core_data({CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: "adafruit"})
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: "nrf52",
        KEY_FRAMEWORK_VERSION: cv.Version(3, 2, 0),
    }
    if required:
        zephyr_require_cdc_acm()
    for port in ports:
        zephyr_claim_cdc_acm(port, f"user{port}")
    _cdc_acm_add_ports()
    return CORE.data[KEY_ZEPHYR]["overlay"][""]


def test_claimed_ports_are_added_and_enabled() -> None:
    overlay = _overlay(0, 1)

    assert overlay.count('status = "okay";') == 2
    assert "cdc_acm_uart0: cdc_acm_uart0 {" in overlay
    assert "cdc_acm_uart1: cdc_acm_uart1 {" in overlay


def test_no_port_without_users() -> None:
    assert "cdc_acm_uart" not in _overlay()


def test_required_port_is_added_without_users() -> None:
    assert "cdc_acm_uart0: cdc_acm_uart0 {" in _overlay(required=True)


def test_required_port_uses_claimed_ports() -> None:
    overlay = _overlay(1, required=True)

    assert "cdc_acm_uart0" not in overlay
    assert "cdc_acm_uart1: cdc_acm_uart1 {" in overlay
