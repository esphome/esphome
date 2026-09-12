"""Tests for the host platform's native-toolchain wiring."""

from __future__ import annotations

import asyncio
from unittest.mock import patch

import pytest

from esphome.components import host
import esphome.config_validation as cv
from esphome.const import (
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    PLATFORM_HOST,
    Toolchain,
)
from esphome.core import CORE, EsphomeError


@pytest.fixture(autouse=True)
def _core_data() -> None:
    CORE.data[KEY_CORE] = {}


def test_schema_resolves_the_host_toolchain() -> None:
    """The host serves exactly one toolchain; it is picked without config."""
    config = host.CONFIG_SCHEMA({})
    assert CORE.toolchain is Toolchain.HOST
    assert CORE.using_native_toolchain
    assert CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] == PLATFORM_HOST
    assert CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] == "host"
    assert "mac_address" in config


def test_schema_rejects_the_platformio_toolchain() -> None:
    """There is no PlatformIO backend left to honor --toolchain platformio."""
    CORE.toolchain = Toolchain.PLATFORMIO
    with pytest.raises(cv.Invalid, match="Unsupported toolchain 'platformio' for host"):
        host.CONFIG_SCHEMA({})


def test_to_code_configures_the_native_build() -> None:
    config = host.CONFIG_SCHEMA({"mac_address": "AA:BB:CC:DD:EE:FF"})
    asyncio.run(host.to_code(config))
    assert "-DUSE_HOST" in CORE.build_flags
    # The standard travels through cpp_standard, not a -std= build flag,
    # so the generator can keep it off the C compile line
    assert CORE.cpp_standard == "gnu++20"
    assert not any(flag.startswith("-std=") for flag in CORE.build_flags)
    assert not CORE.platformio_options
    define_names = {define.name for define in CORE.defines}
    assert {"ESPHOME_BOARD", "ESPHOME_VARIANT", "USE_ESPHOME_HOST_MAC_ADDRESS"} <= (
        define_names
    )


def test_run_compile_hook_claims_the_build() -> None:
    config = {"esphome": {}}
    with patch("esphome.host.toolchain.run_compile", return_value=0) as run:
        assert host.run_compile(object(), config) is True
    run.assert_called_once_with(config, CORE.verbose)


def test_run_compile_hook_raises_on_failure() -> None:
    with (
        patch("esphome.host.toolchain.run_compile", return_value=2),
        pytest.raises(EsphomeError, match="Host build failed"),
    ):
        host.run_compile(object(), {"esphome": {}})
