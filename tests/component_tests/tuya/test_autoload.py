"""Tests for the tuya base-class auto-load path used by other transports."""

import pytest

from esphome.components.tuya import CONFIG_SCHEMA, DOMAIN, FILTER_SOURCE_FILES
from esphome.core import CORE, AutoLoad


def test_autoload_config_passes_through() -> None:
    conf = AutoLoad()

    assert CONFIG_SCHEMA(conf) is conf


def test_filter_source_files_excludes_uart_when_autoloaded(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(CORE, "config", {DOMAIN: AutoLoad()})

    assert set(FILTER_SOURCE_FILES()) == {"tuya_uart.cpp", "tuya_uart.h"}


def test_filter_source_files_includes_uart_when_configured(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    monkeypatch.setattr(CORE, "config", {DOMAIN: {}})

    assert FILTER_SOURCE_FILES() == []
