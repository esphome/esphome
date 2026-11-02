"""The deprecation warning for nRF52 framework versions older than 2.9.2."""

import logging
from unittest.mock import PropertyMock, patch

import pytest

from esphome.components import nrf52
from esphome.const import CONF_FRAMEWORK, CONF_VERSION
from esphome.core import EsphomeCore


@pytest.mark.parametrize(
    ("version", "platformio", "warned"),
    [
        ("2.6.1", False, True),
        ("2.9.1", False, True),
        ("2.9.2", False, False),
        ("3.2.0", False, False),
        ("2.6.1-b", True, False),
        (None, False, False),
    ],
)
def test_warn_old_framework_version(
    caplog: pytest.LogCaptureFixture,
    version: str | None,
    platformio: bool,
    warned: bool,
) -> None:
    framework = {} if version is None else {CONF_VERSION: version}
    with (
        patch.object(
            EsphomeCore,
            "using_toolchain_platformio",
            new_callable=PropertyMock,
            return_value=platformio,
        ),
        caplog.at_level(logging.WARNING),
    ):
        nrf52._warn_old_framework_version({CONF_FRAMEWORK: framework})

    assert ("is deprecated" in caplog.text) is warned
