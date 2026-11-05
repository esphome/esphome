"""Tests for the tuya base-class auto-load path used by other transports."""

import asyncio

import pytest

from esphome.components.tuya import (
    AUTO_LOAD,
    CONFIG_SCHEMA,
    DOMAIN,
    FILTER_SOURCE_FILES,
    to_code,
)
from esphome.core import CORE, AutoLoad


def test_autoload_config_passes_through() -> None:
    conf = AutoLoad()

    assert CONFIG_SCHEMA(conf) is conf


def test_to_code_is_a_noop_when_autoloaded() -> None:
    # A regression here would fall through to UART construction with no
    # controller ID and raise
    assert asyncio.run(to_code(AutoLoad())) is None


def test_auto_load_skips_uart_when_autoloaded() -> None:
    assert AUTO_LOAD(AutoLoad()) == []


def test_auto_load_requests_uart_otherwise() -> None:
    # None is the tooling probe for the maximal set (CI dependency graph)
    assert AUTO_LOAD(None) == ["uart"]
    assert AUTO_LOAD({"id": "x"}) == ["uart"]


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
