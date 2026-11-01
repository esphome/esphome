"""The deprecation warning for nRF52 framework versions older than 2.9.2."""

import logging
from unittest.mock import PropertyMock, patch

import pytest

from esphome.components import nrf52
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, EsphomeCore


@pytest.mark.parametrize(
    ("version", "platformio", "warned"),
    [
        (cv.Version(2, 6, 1), False, True),
        (cv.Version(2, 9, 1), False, True),
        (cv.Version(2, 9, 2), False, False),
        (cv.Version(3, 2, 0), False, False),
        (cv.Version(2, 6, 1, "b"), True, False),
    ],
)
def test_warn_old_framework_version(
    caplog: pytest.LogCaptureFixture,
    version: cv.Version,
    platformio: bool,
    warned: bool,
) -> None:
    with (
        patch.dict(CORE.data, {KEY_CORE: {KEY_FRAMEWORK_VERSION: version}}),
        patch.object(
            EsphomeCore,
            "using_toolchain_platformio",
            new_callable=PropertyMock,
            return_value=platformio,
        ),
        caplog.at_level(logging.WARNING),
    ):
        nrf52._warn_old_framework_version()

    assert ("is deprecated" in caplog.text) is warned
