"""Tests for the prj.conf helpers in the Zephyr component."""

import pytest

from esphome.components.zephyr import (
    zephyr_add_prj_conf,
    zephyr_add_prj_conf_min,
    zephyr_enable_poweroff,
    zephyr_setup_preferences,
)
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


def test_preferences_use_nvs_by_default() -> None:
    zephyr_setup_preferences()

    assert _prj_conf()["CONFIG_NVS"] == (True, True)
    assert _prj_conf()["CONFIG_SETTINGS_NVS"] == (True, True)
    assert "CONFIG_ZMS" not in _prj_conf()


def test_preferences_can_use_zms() -> None:
    zephyr_setup_preferences("ZMS")

    assert _prj_conf()["CONFIG_ZMS"] == (True, True)
    assert _prj_conf()["CONFIG_SETTINGS_ZMS"] == (True, True)
    assert "CONFIG_NVS" not in _prj_conf()


@pytest.mark.parametrize(
    ("integrations", "enabled"), [(set(), True), ({"zigbee"}, False)]
)
def test_poweroff_is_left_out_with_zigbee(
    monkeypatch: pytest.MonkeyPatch, integrations: set[str], enabled: bool
) -> None:
    monkeypatch.setattr(CORE, "loaded_integrations", integrations)
    zephyr_enable_poweroff()

    assert (
        "CONFIG_POWEROFF" in CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF].get("", {})
    ) is enabled
