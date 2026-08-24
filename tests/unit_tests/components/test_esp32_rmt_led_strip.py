import pytest

from esphome.components.esp32_rmt_led_strip.light import (
    CONF_IS_WRGB,
    CONF_RGBW_ORDER,
    _split_rgbw_order,
    _validate_rgbw_order,
    _validate_rgbw_order_exclusivity,
)
import esphome.config_validation as cv
from esphome.const import CONF_IS_RGBW


def test_validate_rgbw_order() -> None:
    assert _validate_rgbw_order("rwgb") == "RWGB"


@pytest.mark.parametrize("rgbw_order", ["RGB", "RRGB", "RGBWW"])
def test_validate_rgbw_order_rejects_invalid_order(rgbw_order: str) -> None:
    with pytest.raises(cv.Invalid, match="permutation of RGBW"):
        _validate_rgbw_order(rgbw_order)


@pytest.mark.parametrize(
    ("rgbw_order", "expected"),
    [
        ("WRGB", ("RGB", 0)),
        ("RWGB", ("RGB", 1)),
        ("GWRB", ("GRB", 1)),
        ("RGBW", ("RGB", 3)),
    ],
)
def test_split_rgbw_order(rgbw_order: str, expected: tuple[str, int]) -> None:
    assert _split_rgbw_order(rgbw_order) == expected


@pytest.mark.parametrize("conflict", [CONF_IS_RGBW, CONF_IS_WRGB])
def test_rgbw_order_is_mutually_exclusive(conflict: str) -> None:
    with pytest.raises(cv.Invalid, match="cannot be used with"):
        _validate_rgbw_order_exclusivity(
            {
                CONF_RGBW_ORDER: "RGBW",
                CONF_IS_RGBW: conflict == CONF_IS_RGBW,
                CONF_IS_WRGB: conflict == CONF_IS_WRGB,
            }
        )


@pytest.mark.parametrize("legacy_option", [CONF_IS_RGBW, CONF_IS_WRGB])
def test_rgbw_order_allows_disabled_legacy_options(legacy_option: str) -> None:
    config = {
        CONF_RGBW_ORDER: "RGBW",
        CONF_IS_RGBW: False,
        CONF_IS_WRGB: False,
    }
    config[legacy_option] = False
    assert _validate_rgbw_order_exclusivity(config) is config
