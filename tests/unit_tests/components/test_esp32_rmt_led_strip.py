import pytest

from esphome.components.esp32_rmt_led_strip.light import (
    CONF_IS_WRGB,
    CONF_RGBW_ORDER,
    _validate_rgbw_order,
    _validate_rgbw_order_exclusivity,
)
import esphome.config_validation as cv
from esphome.const import CONF_IS_RGBW, CONF_RGB_ORDER


def test_validate_rgbw_order() -> None:
    assert _validate_rgbw_order("rwgb") == "RWGB"
    with pytest.raises(cv.Invalid, match="permutation of RGBW"):
        _validate_rgbw_order("RGB")


@pytest.mark.parametrize("conflict", [CONF_RGB_ORDER, CONF_IS_RGBW, CONF_IS_WRGB])
def test_rgbw_order_is_mutually_exclusive(conflict: str) -> None:
    with pytest.raises(cv.Invalid, match="cannot be used with"):
        _validate_rgbw_order_exclusivity({CONF_RGBW_ORDER: "RGBW", conflict: True})
