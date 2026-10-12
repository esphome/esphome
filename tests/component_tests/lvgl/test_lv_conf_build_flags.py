"""``generate_lv_conf_h()`` writes ``#define LV_... 0`` for every LVGL option
the configuration does not use, so ``lv_conf.h`` must leave alone the options
the user defines in build flags. Otherwise the header, which is included after
the compiler ``-D`` flags, turns the option off again.

Build flags can come from ``esphome: build_flags:`` or from the deprecated
``esphome: platformio_options: build_flags:``; both must be honoured.
"""

from __future__ import annotations

import logging

import pytest

from esphome.components.lvgl import defines as df, generate_lv_conf_h
from esphome.const import CONF_BUILD_FLAGS, CONF_ESPHOME, CONF_PLATFORMIO_OPTIONS
from esphome.core import CORE


def _set_esphome_config(
    build_flags: list[str] | None = None,
    pio_build_flags: list[str] | str | None = None,
) -> None:
    pio_options = {} if pio_build_flags is None else {"build_flags": pio_build_flags}
    CORE.config = {
        CONF_ESPHOME: {
            CONF_PLATFORMIO_OPTIONS: pio_options,
            CONF_BUILD_FLAGS: build_flags or [],
        }
    }


def test_unused_define_is_disabled_without_build_flag() -> None:
    _set_esphome_config()
    assert "#define LV_USE_OBSERVER 0" in generate_lv_conf_h().splitlines()


@pytest.mark.parametrize(
    "flags",
    [
        {"build_flags": ["-DLV_USE_OBSERVER=1"]},
        {"build_flags": ["-D LV_USE_OBSERVER"]},
        {"pio_build_flags": ["-DLV_USE_OBSERVER=1"]},
        {"pio_build_flags": "-DLV_USE_OBSERVER=1"},
    ],
    ids=["esphome", "esphome-spaced", "platformio_options", "platformio_options-str"],
)
def test_build_flag_define_is_not_disabled(flags: dict) -> None:
    _set_esphome_config(**flags)
    assert "#define LV_USE_OBSERVER 0" not in generate_lv_conf_h().splitlines()


def test_esphome_build_flag_clash_warns(caplog: pytest.LogCaptureFixture) -> None:
    _set_esphome_config(build_flags=["-DLV_USE_ARC=1"])
    df.add_define("LV_USE_ARC")
    with caplog.at_level(logging.WARNING):
        generate_lv_conf_h()
    assert "LV_USE_ARC" in caplog.text
