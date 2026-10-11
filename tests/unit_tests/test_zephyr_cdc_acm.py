"""Tests for CDC ACM port reservation in the Zephyr component."""

import pytest

from esphome.components.zephyr import zephyr_claim_cdc_acm
from esphome.components.zephyr.const import KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.core import CORE


@pytest.fixture(autouse=True)
def zephyr_data() -> None:
    CORE.data[KEY_ZEPHYR] = {"cdc_acm_users": {}}


def test_different_ports_can_be_claimed() -> None:
    zephyr_claim_cdc_acm(0, "logger")
    zephyr_claim_cdc_acm(1, "usb_cdc_acm")

    assert CORE.data[KEY_ZEPHYR]["cdc_acm_users"] == {0: "logger", 1: "usb_cdc_acm"}


def test_same_user_can_claim_a_port_again() -> None:
    zephyr_claim_cdc_acm(0, "usb_cdc_acm")
    zephyr_claim_cdc_acm(0, "usb_cdc_acm")


def test_second_user_of_a_port_is_rejected() -> None:
    zephyr_claim_cdc_acm(0, "logger")

    with pytest.raises(cv.Invalid, match="'logger' and 'usb_cdc_acm'"):
        zephyr_claim_cdc_acm(0, "usb_cdc_acm")
