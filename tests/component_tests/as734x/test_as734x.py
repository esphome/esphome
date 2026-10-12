"""Tests for the as734x band to counts index mapping and band validation.

The C++ readout maps publish each band from a fixed index, so the order of BANDS_43 is a
contract with as734x.cpp. Reordering either side only moves readings to the wrong entity,
which no build test would notice, so the indices are pinned here.
"""

import pytest

from esphome import config_validation as cv
from esphome.components.as734x import CONF_AS734X_ID, sensor as as734x_sensor
from esphome.config import Config
from esphome.const import CONF_COUNTS, CONF_ID, CONF_TYPE, PlatformFramework
from esphome.core import ID
from tests.component_tests.types import SetCoreConfigCallable

CONFIG = "tests/component_tests/as734x/test_as734x.yaml"

BAND_INDEX = {
    "f1": 0,
    "f2": 1,
    "fz": 2,
    "f3": 3,
    "f4": 4,
    "fy": 5,
    "f5": 6,
    "fxl": 7,
    "f6": 8,
    "f7": 9,
    "f8": 10,
    "nir": 11,
    "clear": 12,
}
AS7341_BANDS = ("f1", "f2", "f3", "f4", "f5", "f6", "f7", "f8", "nir", "clear")


def test_as7341_bands_keep_their_index(generate_main) -> None:
    main_cpp = generate_main(CONFIG)

    for band in AS7341_BANDS:
        assert f"hub41->set_counts_sensor(s41_{band}, {BAND_INDEX[band]});" in main_cpp


def test_as7343_bands_keep_their_index(generate_main) -> None:
    main_cpp = generate_main(CONFIG)

    for band, index in BAND_INDEX.items():
        assert f"hub43->set_counts_sensor(s43_{band}, {index});" in main_cpp


@pytest.mark.parametrize("band", ["fz", "fy", "fxl"])
def test_as7341_rejects_as7343_only_band(
    band: str, set_core_config: SetCoreConfigCallable
) -> None:
    full = Config()
    full["as734x"] = [{CONF_ID: ID("hub41"), CONF_TYPE: "AS7341"}]
    full.declare_ids.append((ID("hub41"), ["as734x", 0, CONF_ID]))
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full)

    with pytest.raises(cv.Invalid, match=f"no {band} band"):
        as734x_sensor.FINAL_VALIDATE_SCHEMA(
            {CONF_AS734X_ID: ID("hub41"), CONF_COUNTS: {"f1": {}, band: {}}}
        )
