"""Tests for the nRF52 sdk-nrf memory analysis hooks."""

import os
from pathlib import Path

import pytest

from esphome.components.nrf52 import toolchain
from esphome.components.nrf52.framework import TOOLCHAIN_VERSION, get_sdk_nrf_tools_path
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE


@pytest.fixture
def nrf52_build(tmp_path: Path) -> Path:
    CORE.name = "test_device"
    CORE.build_path = tmp_path / "build" / "test_device"
    return CORE.build_path / ".pioenvs" / "test_device" / "zephyr"


def _set_sdk_version(version: cv.Version) -> None:
    CORE.data.setdefault(KEY_CORE, {})[KEY_FRAMEWORK_VERSION] = version


def test_binutils_come_from_the_pinned_zephyr_sdk() -> None:
    bin_path = (
        get_sdk_nrf_tools_path()
        / "toolchains"
        / TOOLCHAIN_VERSION
        / "arm-zephyr-eabi"
        / "bin"
    )
    # Windows hosts get the .exe binaries (CI covers both)
    suffix = ".exe" if os.name == "nt" else ""
    assert toolchain.get_objdump_path() == bin_path / f"arm-zephyr-eabi-objdump{suffix}"
    assert toolchain.get_readelf_path() == bin_path / f"arm-zephyr-eabi-readelf{suffix}"


def test_elf_uses_the_nested_layout_from_sdk_2_9_2(nrf52_build: Path) -> None:
    _set_sdk_version(cv.Version(2, 9, 2))
    # A flat image left by an older SDK must not be picked over the current
    # layout, so the choice follows the version rather than what exists
    (nrf52_build / "zephyr.elf").parent.mkdir(parents=True)
    (nrf52_build / "zephyr.elf").write_text("")
    assert toolchain.get_elf_path() == nrf52_build / "zephyr" / "zephyr.elf"


def test_elf_uses_the_flat_layout_before_sdk_2_9_2(nrf52_build: Path) -> None:
    _set_sdk_version(cv.Version(2, 6, 1))
    nested = nrf52_build / "zephyr" / "zephyr.elf"
    nested.parent.mkdir(parents=True)
    nested.write_text("")
    assert toolchain.get_elf_path() == nrf52_build / "zephyr.elf"
