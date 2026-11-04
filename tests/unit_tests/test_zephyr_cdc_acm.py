"""Tests for CDC ACM port reservation in the Zephyr component."""

import pytest

from esphome.components.zephyr import (
    CdcAcmUser,
    _cdc_acm_add_ports,
    _disable_board_cdc_acm_overlay,
    zephyr_claim_cdc_acm,
    zephyr_enable_cdc_acm_rate_callback,
    zephyr_require_cdc_acm,
    zephyr_use_usb_device_next,
)
from esphome.components.zephyr.const import (
    KEY_OVERLAY,
    KEY_OVERLAY_BUILDER,
    KEY_PRJ_CONF,
    KEY_ZEPHYR,
)
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_PLATFORM,
    PLATFORM_NRF52,
)
from esphome.core import CORE


@pytest.fixture(autouse=True)
def clean_zephyr_data() -> None:
    CORE.data[KEY_ZEPHYR] = {"cdc_acm_users": {}, "cdc_acm_required": False}


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


def _prj_conf_for(platform: str, version: cv.Version) -> dict[str, object]:
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: platform,
        KEY_FRAMEWORK_VERSION: version,
    }
    CORE.data[KEY_ZEPHYR] = {
        "cdc_acm_users": {},
        "cdc_acm_required": False,
        KEY_PRJ_CONF: {},
        KEY_OVERLAY: {},
        KEY_OVERLAY_BUILDER: [],
    }
    zephyr_claim_cdc_acm(0, "logger", write_only=True)
    _cdc_acm_add_ports()
    return {
        name: value
        for name, (value, _) in CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF][""].items()
    }


@pytest.mark.parametrize(
    ("version", "expected"),
    [
        (cv.Version(2, 9, 2), False),
        (cv.Version(3, 2, 0), False),
        (cv.Version(3, 4, 0), False),
        (cv.Version(3, 4, 1), True),
        (cv.Version(3, 5, 0), True),
    ],
)
def test_usb_device_next_from_sdk_3_4_1(version: cv.Version, expected: bool) -> None:
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_NRF52,
        KEY_FRAMEWORK_VERSION: version,
    }

    assert zephyr_use_usb_device_next() is expected


def test_legacy_usb_stack_options() -> None:
    prj_conf = _prj_conf_for(PLATFORM_NRF52, cv.Version(3, 2, 0))

    assert prj_conf["CONFIG_USB_DEVICE_STACK"] is True
    assert prj_conf["CONFIG_USB_CDC_ACM"] is True
    assert prj_conf["CONFIG_UART_LINE_CTRL"] is True
    assert "CONFIG_USBD_CDC_ACM_CLASS" not in prj_conf
    assert not CORE.data[KEY_ZEPHYR][KEY_OVERLAY_BUILDER]


def test_new_usb_stack_options() -> None:
    prj_conf = _prj_conf_for(PLATFORM_NRF52, cv.Version(3, 4, 1))

    assert prj_conf["CONFIG_USB_DEVICE_STACK"] is False
    assert prj_conf["CONFIG_USB_DEVICE_STACK_NEXT"] is True
    assert prj_conf["CONFIG_USBD_CDC_ACM_CLASS"] is True
    assert prj_conf["CONFIG_UART_LINE_CTRL"] is True
    # The board's own USB device and CDC ACM port are turned off
    assert prj_conf["CONFIG_CDC_ACM_SERIAL_INITIALIZE_AT_BOOT"] is False
    assert _disable_board_cdc_acm_overlay in CORE.data[KEY_ZEPHYR][KEY_OVERLAY_BUILDER]
    # Legacy-only options do not exist on the new stack
    assert "CONFIG_USB_CDC_ACM" not in prj_conf
    assert "CONFIG_USB_DEVICE_REMOTE_WAKEUP" not in prj_conf
    assert "cdc_acm_uart0" in CORE.data[KEY_ZEPHYR][KEY_OVERLAY][""]


@pytest.mark.parametrize(
    ("version", "legacy_option"),
    [(cv.Version(2, 9, 2), True), (cv.Version(3, 4, 1), False)],
)
def test_rate_callback_options(version: cv.Version, legacy_option: bool) -> None:
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_NRF52,
        KEY_FRAMEWORK_VERSION: version,
    }
    CORE.data[KEY_ZEPHYR] = {KEY_PRJ_CONF: {}}
    zephyr_enable_cdc_acm_rate_callback()

    prj_conf = CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF].get("", {})
    assert ("CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT" in prj_conf) is legacy_option


@pytest.mark.parametrize(
    ("version", "port_added"),
    [(cv.Version(2, 9, 2), False), (cv.Version(3, 4, 1), True)],
)
def test_dfu_without_claimed_port(version: cv.Version, port_added: bool) -> None:
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: PLATFORM_NRF52,
        KEY_FRAMEWORK_VERSION: version,
    }
    CORE.data[KEY_ZEPHYR] = {
        "cdc_acm_users": {},
        "cdc_acm_required": False,
        KEY_PRJ_CONF: {},
        KEY_OVERLAY: {},
        KEY_OVERLAY_BUILDER: [],
    }
    zephyr_require_cdc_acm()
    _cdc_acm_add_ports()

    # The new stack turns off the board's port, so DFU needs one of its own
    assert (
        "cdc_acm_uart0" in CORE.data[KEY_ZEPHYR][KEY_OVERLAY].get("", "")
    ) is port_added
