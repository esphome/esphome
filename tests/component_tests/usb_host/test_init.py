"""Tests for USB host FIFO configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32S3
from esphome.components.usb_host import (
    CONF_NPTX_FIFO_LINES,
    CONF_PTX_FIFO_LINES,
    CONF_RX_FIFO_LINES,
    CONFIG_SCHEMA,
)
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def _configure_esp32_s3(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={
            KEY_BOARD: "esp32-s3-devkitc-1",
            KEY_VARIANT: VARIANT_ESP32S3,
        },
    )


@pytest.mark.parametrize(
    "fifo_settings",
    [
        pytest.param(
            {CONF_RX_FIFO_LINES: 131, CONF_NPTX_FIFO_LINES: 64},
            id="required-fifos",
        ),
        pytest.param(
            {
                CONF_RX_FIFO_LINES: 131,
                CONF_NPTX_FIFO_LINES: 64,
                CONF_PTX_FIFO_LINES: 256,
            },
            id="all-fifos",
        ),
        pytest.param(
            {
                CONF_RX_FIFO_LINES: 1,
                CONF_NPTX_FIFO_LINES: 1,
                CONF_PTX_FIFO_LINES: 0,
            },
            id="minimum-values",
        ),
    ],
)
def test_valid_fifo_settings(
    fifo_settings: dict[str, int], set_core_config: SetCoreConfigCallable
) -> None:
    _configure_esp32_s3(set_core_config)

    config = CONFIG_SCHEMA({"id": "usb_host_id", **fifo_settings})

    for key, value in fifo_settings.items():
        assert config[key] == value


@pytest.mark.parametrize(
    "fifo_settings",
    [
        pytest.param({CONF_RX_FIFO_LINES: 131}, id="missing-nptx"),
        pytest.param({CONF_NPTX_FIFO_LINES: 64}, id="missing-rx"),
        pytest.param({CONF_PTX_FIFO_LINES: 256}, id="ptx-only"),
    ],
)
def test_custom_fifo_settings_require_rx_and_nptx(
    fifo_settings: dict[str, int], set_core_config: SetCoreConfigCallable
) -> None:
    _configure_esp32_s3(set_core_config)

    with pytest.raises(cv.Invalid, match="must both be configured"):
        CONFIG_SCHEMA({"id": "usb_host_id", **fifo_settings})


@pytest.mark.parametrize("invalid_key", [CONF_RX_FIFO_LINES, CONF_NPTX_FIFO_LINES])
def test_required_fifo_sizes_must_be_positive(
    invalid_key: str, set_core_config: SetCoreConfigCallable
) -> None:
    _configure_esp32_s3(set_core_config)
    fifo_settings = {
        CONF_RX_FIFO_LINES: 1,
        CONF_NPTX_FIFO_LINES: 1,
        invalid_key: 0,
    }

    with pytest.raises(cv.Invalid):
        CONFIG_SCHEMA({"id": "usb_host_id", **fifo_settings})
