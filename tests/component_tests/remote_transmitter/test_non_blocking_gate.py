"""non_blocking is family-gated at config validation; the CI build boards never compile
the ISR paths, so this gate is the only CI-reachable coverage for the platform matrix."""

import pytest

from esphome.components.libretiny.const import (
    FAMILY_BK7231N,
    FAMILY_BK7231T,
    FAMILY_BK7238,
    FAMILY_RTL8710B,
    FAMILY_RTL8720C,
    KEY_FAMILY,
    KEY_LIBRETINY,
)
from esphome.components.remote_transmitter import _validate_non_blocking_platform
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from esphome.core import CORE

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize(
    ("platform_framework", "family", "accepted"),
    [
        (PlatformFramework.ESP32_IDF, None, True),
        (PlatformFramework.RTL87XX_ARDUINO, FAMILY_RTL8720C, True),
        (PlatformFramework.RTL87XX_ARDUINO, FAMILY_RTL8710B, False),
        (PlatformFramework.BK72XX_ARDUINO, FAMILY_BK7231N, True),
        (PlatformFramework.BK72XX_ARDUINO, FAMILY_BK7238, True),
        (PlatformFramework.BK72XX_ARDUINO, FAMILY_BK7231T, False),
        (PlatformFramework.ESP8266_ARDUINO, None, False),
    ],
)
def test_non_blocking_platform_gate(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
    family: str | None,
    accepted: bool,
) -> None:
    set_core_config(platform_framework)
    if family is not None:
        CORE.data[KEY_LIBRETINY] = {KEY_FAMILY: family}
    if accepted:
        assert _validate_non_blocking_platform(True) is True
    else:
        with pytest.raises(cv.Invalid, match="non_blocking is only supported on"):
            _validate_non_blocking_platform(True)
