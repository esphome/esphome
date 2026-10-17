"""Tests for the runtime_image hub configuration."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.runtime_image import (
    CONF_JPEG_DECODER,
    CONFIG_SCHEMA,
    DECODER_JPEGDEC,
    DECODER_LIBJPEG_TURBO,
    _get_data,
)
from esphome.const import PlatformFramework, Toolchain
from esphome.core import CORE

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize(
    ("platform_framework", "toolchain"),
    [
        (PlatformFramework.ESP32_IDF, Toolchain.ESP_IDF),
        (PlatformFramework.HOST_NATIVE, Toolchain.HOST),
    ],
)
def test_libjpeg_turbo_accepted(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
    toolchain: Toolchain,
) -> None:
    """libjpeg-turbo is accepted where the build can fetch or link it."""
    set_core_config(platform_framework)
    CORE.toolchain = toolchain

    CONFIG_SCHEMA({CONF_JPEG_DECODER: "libjpeg_turbo"})

    assert _get_data().jpeg_decoder == DECODER_LIBJPEG_TURBO


@pytest.mark.parametrize(
    ("platform_framework", "toolchain"),
    [
        (PlatformFramework.ESP32_IDF, Toolchain.PLATFORMIO),
        (PlatformFramework.ESP32_ARDUINO, Toolchain.PLATFORMIO),
        (PlatformFramework.ESP8266_ARDUINO, Toolchain.PLATFORMIO),
        (PlatformFramework.RP2040_ARDUINO, Toolchain.PLATFORMIO),
    ],
)
def test_libjpeg_turbo_rejected(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
    toolchain: Toolchain,
) -> None:
    """Elsewhere the dependency would be dropped, so the config is rejected up front."""
    set_core_config(platform_framework)
    CORE.toolchain = toolchain

    with pytest.raises(cv.Invalid, match="only supported on ESP32"):
        CONFIG_SCHEMA({CONF_JPEG_DECODER: DECODER_LIBJPEG_TURBO})


def test_jpegdec_is_default(set_core_config: SetCoreConfigCallable) -> None:
    """Without jpeg_decoder, JPEGDEC is used, also on targets that reject libjpeg-turbo."""
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    CORE.toolchain = Toolchain.PLATFORMIO

    assert CONFIG_SCHEMA({})[CONF_JPEG_DECODER] == DECODER_JPEGDEC
    assert _get_data().jpeg_decoder == DECODER_JPEGDEC
