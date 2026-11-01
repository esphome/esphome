"""Tests for CDC ACM port reservation in the Zephyr component."""

import pytest

from esphome.components.zephyr import (
    CdcAcmUser,
    _cdc_acm_add_ports,
    zephyr_add_board_cdc_acm_node,
    zephyr_claim_cdc_acm,
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


def _overlay_with_board_node(board_path: str, *ports: int) -> str:
    zephyr_set_core_data({CONF_BOARD: "xiao_ble", KEY_BOOTLOADER: "adafruit"})
    CORE.data[KEY_CORE] = {
        KEY_TARGET_PLATFORM: "nrf52",
        KEY_FRAMEWORK_VERSION: cv.Version(3, 2, 0),
    }
    zephyr_add_board_cdc_acm_node(board_path)
    zephyr_add_board_cdc_acm_node(board_path)
    for port in ports:
        zephyr_claim_cdc_acm(port, f"user{port}")
    _cdc_acm_add_ports()
    return CORE.data[KEY_ZEPHYR]["overlay"][""]


def test_board_cdc_acm_node_is_disabled_before_own_ports() -> None:
    overlay = _overlay_with_board_node("/soc/usbd@40027000/board_cdc_acm_uart", 0, 1)

    assert overlay.count("&{/soc/usbd@40027000/board_cdc_acm_uart} {") == 1
    disabled = overlay.index('status = "disabled";')
    assert disabled < overlay.index("&zephyr_udc0 {")
    assert overlay.count('status = "okay";') == 2
    assert "cdc_acm_uart0: cdc_acm_uart0 {" in overlay
    assert "cdc_acm_uart1: cdc_acm_uart1 {" in overlay


def test_board_cdc_acm_node_is_kept_without_claimed_ports() -> None:
    overlay = _overlay_with_board_node("/soc/usbd@40027000/board_cdc_acm_uart")
    assert "board_cdc_acm_uart" not in overlay
