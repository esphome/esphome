"""Tests for the per-type channel limit, which depends on the target."""

from pathlib import Path
import subprocess
import sys

import pytest

from esphome.components.esp32.const import KEY_VARIANT, VARIANT_ESP32, VARIANT_ESP32P4
from esphome.components.usb_host import CONF_INTERFACE_DESCRIPTION
from esphome.components.usb_uart import CONFIG_SCHEMA, channel_schema, uart_types
import esphome.config_validation as cv
from esphome.const import CONF_BAUD_RATE, CONF_CHANNELS, CONF_TYPE, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

CH34X = next(t for t in uart_types if t.name == "CH34X")


def test_import_needs_no_core_config() -> None:
    """The C++ unit test runner imports every component before any config is loaded."""
    subprocess.run(
        [sys.executable, "-c", "from esphome.components import usb_uart"],
        cwd=Path(__file__).parents[3],
        check=True,
    )


@pytest.mark.parametrize(
    ("variant", "limit"), [(VARIANT_ESP32, 3), (VARIANT_ESP32P4, 4)]
)
def test_channel_limit_follows_the_target(
    set_core_config: SetCoreConfigCallable, variant: str, limit: int
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: variant})
    schema = channel_schema(CH34X)
    channel = {CONF_BAUD_RATE: 9600}
    assert len(schema({CONF_CHANNELS: [channel] * limit})[CONF_CHANNELS]) == limit
    with pytest.raises(cv.Invalid, match=f"maximum of {limit} channels"):
        schema({CONF_CHANNELS: [channel] * (limit + 1)})


@pytest.mark.parametrize(
    "config",
    [
        pytest.param(
            [
                {
                    CONF_TYPE: "cp210x",
                    CONF_CHANNELS: [{CONF_BAUD_RATE: 9600}, {CONF_BAUD_RATE: 9600}],
                }
            ],
            id="unfiltered_two_channels",
        ),
        pytest.param(
            [
                {
                    CONF_TYPE: "cp210x",
                    CONF_INTERFACE_DESCRIPTION: "HubZ ZigBee Com Port",
                    CONF_CHANNELS: [{CONF_BAUD_RATE: 9600}],
                }
            ],
            id="filtered_one_channel",
        ),
    ],
)
def test_supported_channel_counts_are_accepted(
    set_core_config: SetCoreConfigCallable, config: list[dict]
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )
    validated = CONFIG_SCHEMA(config)
    assert len(validated[0][CONF_CHANNELS]) == len(config[0][CONF_CHANNELS])


def test_filtered_entry_with_two_channels_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )
    config = [
        {
            CONF_TYPE: "cp210x",
            CONF_INTERFACE_DESCRIPTION: "HubZ ZigBee Com Port",
            CONF_CHANNELS: [{CONF_BAUD_RATE: 9600}, {CONF_BAUD_RATE: 9600}],
        }
    ]
    with pytest.raises(cv.Invalid, match="Only one channel") as exc_info:
        CONFIG_SCHEMA(config)
    assert exc_info.value.path[-1] == CONF_CHANNELS
