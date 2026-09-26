"""Tests for the nRF52 sdk-nrf memory analysis hooks."""

from pathlib import Path

import pytest

from esphome.components.nrf52 import toolchain
from esphome.components.nrf52.framework import TOOLCHAIN_VERSION, get_sdk_nrf_tools_path
from esphome.core import CORE


@pytest.fixture
def nrf52_build(tmp_path: Path) -> Path:
    CORE.name = "test_device"
    CORE.build_path = tmp_path / "build" / "test_device"
    return CORE.build_path / ".pioenvs" / "test_device" / "zephyr"


def test_binutils_come_from_the_pinned_zephyr_sdk() -> None:
    bin_path = (
        get_sdk_nrf_tools_path()
        / "toolchains"
        / TOOLCHAIN_VERSION
        / "arm-zephyr-eabi"
        / "bin"
    )
    assert toolchain.get_objdump_path() == bin_path / "arm-zephyr-eabi-objdump"
    assert toolchain.get_readelf_path() == bin_path / "arm-zephyr-eabi-readelf"


def test_elf_prefers_the_nested_sdk_layout(nrf52_build: Path) -> None:
    nested = nrf52_build / "zephyr" / "zephyr.elf"
    nested.parent.mkdir(parents=True)
    nested.write_text("")
    (nrf52_build / "zephyr.elf").write_text("")
    assert toolchain.get_elf_path() == nested


def test_elf_falls_back_to_the_flat_layout(nrf52_build: Path) -> None:
    # SDKs before 2.9.2 wrote the image one level up; a missing ELF is
    # reported by the caller under this path
    assert toolchain.get_elf_path() == nrf52_build / "zephyr.elf"
