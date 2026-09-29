"""Tests for the Arduino framework version floor."""

import pytest

from esphome.components.esp8266 import (
    _arduino_check_versions,
    framework_package_version,
)
import esphome.config_validation as cv
from esphome.const import CONF_PLATFORM_VERSION, CONF_VERSION
from esphome.core import EsphomeError


def test_versions_before_3_are_rejected() -> None:
    with pytest.raises(cv.Invalid, match="no longer supported") as excinfo:
        _arduino_check_versions({CONF_VERSION: "2.7.4"})
    assert excinfo.value.path == [CONF_VERSION]


def test_supported_versions_pass() -> None:
    value = _arduino_check_versions({CONF_VERSION: "3.0.2"})
    assert value[CONF_VERSION] == "3.0.2"
    assert "espressif8266@3.2.0" in value[CONF_PLATFORM_VERSION]

    value = _arduino_check_versions({CONF_VERSION: "recommended"})
    assert value[CONF_VERSION] == "3.1.2"
    assert "espressif8266@4.2.1" in value[CONF_PLATFORM_VERSION]


def test_framework_package_version() -> None:
    assert framework_package_version(cv.Version(3, 1, 2)) == "3.30102.0"
    assert framework_package_version(cv.Version(3, 2, 0)) == "3.30200.0"
    # A future major bump needs its own encoding, not a doomed registry lookup
    with pytest.raises(EsphomeError, match="not supported yet"):
        framework_package_version(cv.Version(4, 0, 0))
    # Cores before 3.x cannot build ESPHome (C++20) and are rejected
    with pytest.raises(EsphomeError, match="requires core 3"):
        framework_package_version(cv.Version(2, 7, 4))
