"""Each SoC variant stores preferences in the storage made for its memory."""

import pytest

from esphome.components import nrf52
from esphome.components.nrf52.const import (
    VARIANT_NRF54L15,
    VARIANT_NRF54LM20,
    VARIANT_NRF51822,
    VARIANT_NRF52832,
    VARIANT_NRF52840,
)
from esphome.components.zephyr import zephyr_setup_preferences
from esphome.components.zephyr.const import KEY_PRJ_CONF, KEY_ZEPHYR
from esphome.core import CORE


@pytest.mark.parametrize(
    ("variant", "storage", "other"),
    [
        (VARIANT_NRF54L15, "ZMS", "NVS"),
        (VARIANT_NRF54LM20, "ZMS", "NVS"),
        (VARIANT_NRF52840, "NVS", "ZMS"),
        (VARIANT_NRF52832, "NVS", "ZMS"),
        (VARIANT_NRF51822, "NVS", "ZMS"),
    ],
)
def test_variant_picks_the_settings_backend(
    variant: str, storage: str, other: str
) -> None:
    CORE.data[KEY_ZEPHYR] = {KEY_PRJ_CONF: {}}

    zephyr_setup_preferences(nrf52._preferences_storage(variant))

    prj_conf = CORE.data[KEY_ZEPHYR][KEY_PRJ_CONF][""]
    assert prj_conf[f"CONFIG_{storage}"] == (True, True)
    assert prj_conf[f"CONFIG_SETTINGS_{storage}"] == (True, True)
    assert f"CONFIG_{other}" not in prj_conf
    assert f"CONFIG_SETTINGS_{other}" not in prj_conf
