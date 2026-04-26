from __future__ import annotations

import asyncio
from pathlib import Path
import runpy

import pytest

from esphome.components import nrf52
from esphome.components.zephyr import zephyr_data
from esphome.components.zephyr.const import KEY_EXTRA_BUILD_FILES, KEY_PM_STATIC
import esphome.config_validation as cv
from esphome.const import KEY_CORE
from esphome.core import CORE

NRF52_DIR = Path(__file__).parents[2] / "esphome" / "components" / "nrf52"


def _load_artifact_script() -> dict:
    return runpy.run_path(str(NRF52_DIR / "xiao_ble_mcuboot_artifact.py.script"))


def _setup_core(path: Path) -> None:
    CORE.config_path = path / "test.yaml"
    CORE.name = "test"
    CORE.build_path = path / ".esphome" / "build" / "test"
    CORE.data[KEY_CORE] = {}


def test_usb_cdc_recovery_validates_for_xiao_ble_mcuboot(setup_core: Path) -> None:
    _setup_core(setup_core)

    config = nrf52.CONFIG_SCHEMA(
        {
            "board": "xiao_ble",
            "bootloader": "mcuboot",
            "mcuboot": {"usb_cdc_recovery": True},
        }
    )

    assert config["mcuboot"]["usb_cdc_recovery"] is True


def test_usb_cdc_recovery_rejects_adafruit_bootloader(setup_core: Path) -> None:
    _setup_core(setup_core)

    with pytest.raises(cv.Invalid, match="only valid with bootloader: mcuboot"):
        nrf52.CONFIG_SCHEMA(
            {
                "board": "xiao_ble",
                "bootloader": "adafruit_nrf52_sd140_v7",
                "mcuboot": {"usb_cdc_recovery": True},
            }
        )


def test_usb_cdc_recovery_rejects_unsupported_board(setup_core: Path) -> None:
    _setup_core(setup_core)

    with pytest.raises(cv.Invalid, match="only supported on xiao_ble"):
        nrf52.CONFIG_SCHEMA(
            {
                "board": "adafruit_feather_nrf52840",
                "bootloader": "mcuboot",
                "mcuboot": {"usb_cdc_recovery": True},
            }
        )


def test_usb_cdc_recovery_registers_child_image_files(setup_core: Path) -> None:
    _setup_core(setup_core)
    config = nrf52.CONFIG_SCHEMA(
        {
            "board": "xiao_ble",
            "bootloader": "mcuboot",
            "mcuboot": {"usb_cdc_recovery": True},
        }
    )

    asyncio.run(nrf52.to_code(config))
    CORE.flush_tasks()

    extra_build_files = zephyr_data()[KEY_EXTRA_BUILD_FILES]
    assert (
        extra_build_files["zephyr/child_image/mcuboot.conf"].name
        == "xiao_ble_mcuboot_usb_cdc_recovery.conf"
    )
    assert (
        extra_build_files["zephyr/child_image/mcuboot/boards/xiao_ble.overlay"].name
        == "xiao_ble_mcuboot_usb_cdc_recovery.overlay"
    )
    assert (
        "post:xiao_ble_mcuboot_artifact.py" in CORE.platformio_options["extra_scripts"]
    )
    assert [
        (section.name, section.address, section.size)
        for section in zephyr_data()[KEY_PM_STATIC]
    ] == [
        ("mcuboot_secondary", 0x79000, 0x79000),
        ("settings_storage", 0xF2000, 0x2000),
        ("mcuboot", 0xF4000, 0xB000),
        ("empty_mbr_params_page", 0xFF000, 0x1000),
    ]


def test_usb_cdc_recovery_child_image_files_contain_required_settings() -> None:
    conf = (NRF52_DIR / "xiao_ble_mcuboot_usb_cdc_recovery.conf").read_text(
        encoding="utf-8"
    )
    overlay = (NRF52_DIR / "xiao_ble_mcuboot_usb_cdc_recovery.overlay").read_text(
        encoding="utf-8"
    )

    for setting in (
        "CONFIG_CONSOLE=n",
        "CONFIG_UART_CONSOLE=n",
        "CONFIG_LOG=n",
        "CONFIG_PRINTK=n",
        "CONFIG_HW_CC3XX=n",
        "CONFIG_BOOT_BANNER=n",
        "CONFIG_SINGLE_APPLICATION_SLOT=y",
        "CONFIG_BOOT_VALIDATE_SLOT0=n",
        "CONFIG_BOOT_UPGRADE_ONLY=y",
        "CONFIG_MCUBOOT_SERIAL=y",
        "CONFIG_BOOT_SERIAL_ENTRANCE_GPIO=n",
        "CONFIG_BOOT_SERIAL_CDC_ACM=y",
        "CONFIG_BOOT_ECDSA_TINYCRYPT=y",
        "CONFIG_BOOT_SERIAL_IMG_GRP_HASH=n",
        "CONFIG_BOOT_SERIAL_WAIT_FOR_DFU=y",
        "CONFIG_BOOT_SERIAL_WAIT_FOR_DFU_TIMEOUT=30000",
        "CONFIG_BOOT_ERASE_PROGRESSIVELY=y",
        "CONFIG_BOOT_WATCHDOG_FEED=n",
    ):
        assert setting in conf
    assert "zephyr,cdc-acm-uart = &cdc_acm_uart0;" in overlay
    assert 'compatible = "zephyr,cdc-acm-uart";' in overlay
    assert "&gpio0" in overlay
    assert "&gpio1" in overlay


def test_filter_xiao_ble_mcuboot_updater_input_keeps_only_expected_ranges(
    tmp_path: Path,
) -> None:
    script = _load_artifact_script()
    input_path = tmp_path / "merged.hex"
    output_path = tmp_path / "xiao_ble_mcuboot_updater_input.hex"
    source_data = {
        0x00000000: 0xAA,
        0x000F3FFF: 0xBB,
        0x000F4000: 0x11,
        0x000F4001: 0x22,
        0x000FEFFF: 0x33,
        0x000FF000: 0xCC,
        0x10001014: 0x00,
        0x10001015: 0x40,
        0x10001016: 0x0F,
        0x10001017: 0x00,
        0x10001018: 0x00,
        0x10001019: 0xF0,
        0x1000101A: 0x0F,
        0x1000101B: 0x00,
    }
    script["write_intel_hex"](input_path, source_data)

    script["filter_xiao_ble_mcuboot_updater_input"](input_path, output_path)

    output_data = script["read_intel_hex"](output_path)
    assert output_data == {
        0x000F4000: 0x11,
        0x000F4001: 0x22,
        0x000FEFFF: 0x33,
        0x10001014: 0x00,
        0x10001015: 0x40,
        0x10001016: 0x0F,
        0x10001017: 0x00,
        0x10001018: 0x00,
        0x10001019: 0xF0,
        0x1000101A: 0x0F,
        0x1000101B: 0x00,
    }


def test_filter_xiao_ble_mcuboot_updater_input_rejects_wrong_uicr(
    tmp_path: Path,
) -> None:
    script = _load_artifact_script()
    input_path = tmp_path / "merged.hex"
    output_path = tmp_path / "xiao_ble_mcuboot_updater_input.hex"
    source_data = {
        0x000F4000: 0x11,
        0x10001014: 0x00,
        0x10001015: 0x40,
        0x10001016: 0x0F,
        0x10001017: 0x00,
        0x10001018: 0x00,
        0x10001019: 0xE0,
        0x1000101A: 0x0F,
        0x1000101B: 0x00,
    }
    script["write_intel_hex"](input_path, source_data)

    with pytest.raises(script["IntelHexError"], match="expected 0x000FF000"):
        script["filter_xiao_ble_mcuboot_updater_input"](input_path, output_path)


def test_filter_xiao_ble_mcuboot_updater_input_injects_expected_uicr(
    tmp_path: Path,
) -> None:
    script = _load_artifact_script()
    input_path = tmp_path / "merged.hex"
    output_path = tmp_path / "xiao_ble_mcuboot_updater_input.hex"
    script["write_intel_hex"](input_path, {0x000F4000: 0x11})

    script["filter_xiao_ble_mcuboot_updater_input"](
        input_path, output_path, inject_uicr=True
    )

    output_data = script["read_intel_hex"](output_path)
    assert output_data == {
        0x000F4000: 0x11,
        0x10001014: 0x00,
        0x10001015: 0x40,
        0x10001016: 0x0F,
        0x10001017: 0x00,
        0x10001018: 0x00,
        0x10001019: 0xF0,
        0x1000101A: 0x0F,
        0x1000101B: 0x00,
    }
