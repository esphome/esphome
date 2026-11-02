"""Tests for the prj.conf helpers in the Zephyr component."""

import pytest

from esphome.components.zephyr import zephyr_add_prj_conf, zephyr_add_prj_conf_min
from esphome.components.zephyr.const import KEY_PRJ_CONF, KEY_ZEPHYR
from esphome.core import CORE


@pytest.fixture(autouse=True)
def clean_zephyr_data() -> None:
    CORE.data[KEY_ZEPHYR] = {KEY_PRJ_CONF: {}}


def _prj_conf() -> dict:
    return CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF][""]


def test_min_overrides_an_optional_default() -> None:
    zephyr_add_prj_conf("MAIN_STACK_SIZE", 2048, required=False)
    zephyr_add_prj_conf_min("MAIN_STACK_SIZE", 8192)

    assert _prj_conf()["CONFIG_MAIN_STACK_SIZE"] == (8192, True)


def test_largest_min_wins_in_any_order() -> None:
    zephyr_add_prj_conf_min("MAIN_STACK_SIZE", 8192)
    zephyr_add_prj_conf_min("MAIN_STACK_SIZE", 4096)

    assert _prj_conf()["CONFIG_MAIN_STACK_SIZE"] == (8192, True)
