"""Tests for the flash layout checks run by sysbuild (zephyr/partition_checks.cmake).

The checks run under `cmake -P` against a fake devicetree: dt_* and sysbuild_get are
replaced by functions that read the image layouts each test builds.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
import shutil
import subprocess

import pytest

from esphome.components.zephyr.partitions import (
    PARTITION_CHECKS_CMAKE,
    BootLayout,
    render_check_call,
    render_sysbuild_cmake,
)

FLASH = "/soc/flash-controller@4001e000/flash@0"
DATA_FLASH = "/soc/flash-controller@407e0000/flash@40100000"

_FAKES = """
function(_fake_key out kind image name)
  string(MAKE_C_IDENTIFIER "FAKE_${kind}_${image}_${name}" key)
  set(${out} ${key} PARENT_SCOPE)
endfunction()
function(_fake_get var kind image name)
  _fake_key(key ${kind} ${image} "${name}")
  if(DEFINED ${key})
    set(${var} "${${key}}" PARENT_SCOPE)
  else()
    unset(${var} PARENT_SCOPE)
  endif()
endfunction()
function(dt_nodelabel var)
  cmake_parse_arguments(A "" "TARGET;NODELABEL" "" ${ARGN})
  _fake_get(value label ${A_TARGET} "${A_NODELABEL}")
  set(${var} "${value}" PARENT_SCOPE)
  if(NOT DEFINED value)
    unset(${var} PARENT_SCOPE)
  endif()
endfunction()
function(dt_chosen var)
  cmake_parse_arguments(A "" "TARGET;PROPERTY" "" ${ARGN})
  _fake_get(value chosen ${A_TARGET} "${A_PROPERTY}")
  set(${var} "${value}" PARENT_SCOPE)
  if(NOT DEFINED value)
    unset(${var} PARENT_SCOPE)
  endif()
endfunction()
function(dt_prop var)
  cmake_parse_arguments(A "" "TARGET;PATH;PROPERTY" "" ${ARGN})
  _fake_get(value prop ${A_TARGET} "${A_PATH}_${A_PROPERTY}")
  set(${var} "${value}" PARENT_SCOPE)
  if(NOT DEFINED value)
    unset(${var} PARENT_SCOPE)
  endif()
endfunction()
function(sysbuild_get var)
  cmake_parse_arguments(A "KCONFIG" "IMAGE;VAR" "" ${ARGN})
  _fake_get(value kconfig ${A_IMAGE} "${A_VAR}")
  set(${var} "${value}" PARENT_SCOPE)
endfunction()
"""


@dataclass
class Image:
    """One sysbuild image's devicetree: partitions by label, plus chosen nodes."""

    partitions: dict[str, tuple[int, int]] = field(default_factory=dict)
    code: str | None = None
    settings: str | None = None
    devices: dict[str, str] = field(default_factory=dict)  # label -> flash node
    kconfig: dict[str, str] = field(default_factory=dict)
    erase_block: int | None = None

    def path(self, label: str) -> str:
        start = self.partitions[label][0]
        return f"{self.devices.get(label, FLASH)}/partitions/partition@{start:x}"


def _image_cmake(name: str, image: Image) -> list[str]:
    def put(kind: str, key: str, value: str) -> str:
        return f'_fake_key(k {kind} {name} "{key}")\nset(${{k}} "{value}")'

    lines = []
    for label, (start, size) in image.partitions.items():
        path = image.path(label)
        lines.append(put("label", label, path))
        lines.append(put("prop", f"{path}_reg", f"{start};{size}"))
    if image.code is not None:
        lines.append(put("chosen", "zephyr,code-partition", image.path(image.code)))
    else:
        lines.append(put("chosen", "zephyr,flash", FLASH))
    if image.settings is not None:
        lines.append(
            put("chosen", "zephyr,settings-partition", image.path(image.settings))
        )
    if image.erase_block is not None:
        lines.append(put("prop", f"{FLASH}_erase-block-size", str(image.erase_block)))
    for key, value in {"CONFIG_SETTINGS_NVS": "y", **image.kconfig}.items():
        lines.append(put("kconfig", key, value))
    return lines


def _run(
    tmp_path: Path,
    images: dict[str, Image],
    sysbuild: dict[str, str] | None = None,
    boot: BootLayout | None = None,
    required: list[tuple[str, str]] | None = None,
) -> subprocess.CompletedProcess:
    lines = [
        "cmake_minimum_required(VERSION 3.20)",
        "set(DEFAULT_IMAGE app)",
        f"set(IMAGES {' '.join(images)})",
        *(f"set({key} {value})" for key, value in (sysbuild or {}).items()),
        _FAKES,
    ]
    for name, image in images.items():
        lines += _image_cmake(name, image)
    lines += [
        PARTITION_CHECKS_CMAKE.read_text(encoding="utf-8"),
        render_check_call(boot, required or []),
    ]
    script = tmp_path / "check.cmake"
    script.write_text("\n".join(lines), encoding="utf-8")
    return subprocess.run(
        ["cmake", "-P", str(script)], capture_output=True, text=True, check=False
    )


def _ok(result: subprocess.CompletedProcess) -> None:
    assert result.returncode == 0, result.stderr


def _fails(result: subprocess.CompletedProcess, *fragments: str) -> None:
    assert result.returncode != 0
    message = " ".join(result.stderr.split())
    for fragment in fragments:
        assert fragment in message, message


MCUBOOT = {"SB_CONFIG_BOOTLOADER_MCUBOOT": "y"}
STOCK = {
    "boot_partition": (0x0, 0xC000),
    "slot0_partition": (0xC000, 0x76000),
    "slot1_partition": (0x82000, 0x76000),
    "storage_partition": (0xF8000, 0x8000),
}


def _mcuboot_images(
    app: dict | None = None, mcuboot: dict | None = None
) -> dict[str, Image]:
    return {
        "app": Image(partitions=app or dict(STOCK), code="slot0_partition"),
        "mcuboot": Image(partitions=mcuboot or dict(STOCK), code="boot_partition"),
    }


pytestmark = pytest.mark.skipif(shutil.which("cmake") is None, reason="needs cmake")


def test_stock_mcuboot_layout_passes(tmp_path: Path) -> None:
    _ok(_run(tmp_path, _mcuboot_images(), MCUBOOT, BootLayout(0x0)))


def test_mcuboot_needs_slot1(tmp_path: Path) -> None:
    layout = {k: v for k, v in STOCK.items() if k != "slot1_partition"}
    _fails(
        _run(tmp_path, _mcuboot_images(layout, layout), MCUBOOT),
        "'slot1_partition' in both the app and mcuboot images",
    )


def test_app_and_mcuboot_must_agree(tmp_path: Path) -> None:
    app = {**STOCK, "slot1_partition": (0x80000, 0x74000)}
    _fails(
        _run(tmp_path, _mcuboot_images(app=app), MCUBOOT),
        "disagree on 'slot1_partition'",
        "0x80000-0xf4000 in the app, 0x82000-0xf8000 in mcuboot",
    )


def test_single_app_rejects_slot1(tmp_path: Path) -> None:
    sysbuild = {**MCUBOOT, "SB_CONFIG_MCUBOOT_MODE_SINGLE_APP": "y"}
    _fails(_run(tmp_path, _mcuboot_images(), sysbuild), "without slot1_partition")


def test_single_app_without_slot1_passes(tmp_path: Path) -> None:
    sysbuild = {**MCUBOOT, "SB_CONFIG_MCUBOOT_MODE_SINGLE_APP": "y"}
    layout = {**STOCK, "slot0_partition": (0xC000, 0xEC000)}
    del layout["slot1_partition"]
    _ok(_run(tmp_path, _mcuboot_images(layout, layout), sysbuild))


def test_swap_scratch_needs_scratch(tmp_path: Path) -> None:
    sysbuild = {**MCUBOOT, "SB_CONFIG_MCUBOOT_MODE_SWAP_SCRATCH": "y"}
    _fails(_run(tmp_path, _mcuboot_images(), sysbuild), "'scratch_partition'")


def test_app_must_run_from_slot0(tmp_path: Path) -> None:
    images = _mcuboot_images()
    images["app"].code = "slot1_partition"
    _fails(_run(tmp_path, images, MCUBOOT), "must run from slot0_partition")


def test_mcuboot_must_run_from_boot_partition(tmp_path: Path) -> None:
    images = _mcuboot_images()
    images["mcuboot"].code = "slot0_partition"
    _fails(_run(tmp_path, images, MCUBOOT), "MCUboot must run from boot_partition")


def test_direct_xip_second_image_runs_from_slot1(tmp_path: Path) -> None:
    images = _mcuboot_images()
    images["app_slot1_variant"] = Image(partitions=dict(STOCK), code="slot0_partition")
    _fails(_run(tmp_path, images, MCUBOOT), "Direct-xip")
    images["app_slot1_variant"].code = "slot1_partition"
    _ok(_run(tmp_path, images, MCUBOOT))


def test_mcuboot_must_start_at_boot_start(tmp_path: Path) -> None:
    _fails(
        _run(tmp_path, _mcuboot_images(), MCUBOOT, BootLayout(0x1000)),
        "MCUboot must start at 0x1000",
        "it starts at 0x0",
    )


def test_app_without_bootloader_must_start_at_boot_start(tmp_path: Path) -> None:
    images = {"app": Image(partitions=dict(STOCK), code="slot0_partition")}
    _fails(
        _run(tmp_path, images, boot=BootLayout(0x0)),
        "The application must start at 0x0",
        "it starts at 0xc000",
    )
    images["app"].partitions["slot0_partition"] = (0x0, 0xF8000)
    del images["app"].partitions["boot_partition"]
    _ok(_run(tmp_path, images, boot=BootLayout(0x0)))


def test_app_without_code_partition_starts_at_flash_start(tmp_path: Path) -> None:
    images = {"app": Image(partitions={"storage_partition": (0xF8000, 0x8000)})}
    _ok(_run(tmp_path, images, boot=BootLayout(0x0)))
    _fails(_run(tmp_path, images, boot=BootLayout(0x4000)), "must start at 0x4000")


def test_reserved_ranges_are_kept_free(tmp_path: Path) -> None:
    adafruit = BootLayout(0x26000, ((0x0, 0x26000), (0xF4000, 0x100000)))
    partitions = {
        "code_partition": (0x26000, 0xC6000),
        "storage_partition": (0xEC000, 0x8000),
        "boot_partition": (0xF4000, 0xC000),  # UF2; not written without MCUboot
    }
    images = {"app": Image(partitions=partitions, code="code_partition")}
    _ok(_run(tmp_path, images, boot=adafruit))
    partitions["storage_partition"] = (0xF0000, 0x8000)
    _fails(_run(tmp_path, images, boot=adafruit), "overlaps 0xf4000-0x100000")


def test_storage_is_required(tmp_path: Path) -> None:
    layout = {k: v for k, v in STOCK.items() if k != "storage_partition"}
    _fails(
        _run(tmp_path, _mcuboot_images(layout, layout), MCUBOOT),
        "Preferences need a flash partition labeled 'storage_partition'",
    )


def test_settings_partition_chosen_node_counts(tmp_path: Path) -> None:
    layout = dict(STOCK)
    layout["settings"] = layout.pop("storage_partition")
    images = _mcuboot_images(layout, layout)
    images["app"].settings = "settings"
    _ok(_run(tmp_path, images, MCUBOOT))


def test_storage_not_needed_without_flash_settings(tmp_path: Path) -> None:
    images = {
        "app": Image(
            partitions={"slot0_partition": (0x0, 0x10000)},
            code="slot0_partition",
            kconfig={"CONFIG_SETTINGS_NVS": "n"},
        )
    }
    _ok(_run(tmp_path, images))


def test_nvs_needs_two_sectors(tmp_path: Path) -> None:
    images = _mcuboot_images()
    images["app"].erase_block = 0x1000
    images["app"].kconfig = {"CONFIG_SETTINGS_NVS_SECTOR_SIZE_MULT": "8"}
    _fails(_run(tmp_path, images, MCUBOOT), "NVS needs at least 65536")
    images["app"].kconfig = {"CONFIG_SETTINGS_NVS_SECTOR_SIZE_MULT": "1"}
    _ok(_run(tmp_path, images, MCUBOOT))


def test_overlapping_partitions_fail(tmp_path: Path) -> None:
    layout = {**STOCK, "storage_partition": (0xF0000, 0x10000)}
    _fails(
        _run(tmp_path, _mcuboot_images(layout, layout), MCUBOOT),
        "partition@82000",
        "partition@f0000",
        "overlap",
    )


def test_partitions_on_other_flash_devices_do_not_overlap(tmp_path: Path) -> None:
    # ek_ra4m1 keeps storage in data flash at offset 0.
    image = Image(
        partitions={"storage_partition": (0x0, 0x2000)},
        devices={"storage_partition": DATA_FLASH},
    )
    _ok(_run(tmp_path, {"app": image}, boot=BootLayout(0x0)))


def test_required_partitions(tmp_path: Path) -> None:
    required = [("zboss_nvram", "Zigbee")]
    _fails(
        _run(tmp_path, _mcuboot_images(), MCUBOOT, required=required),
        "Zigbee needs a flash partition labeled 'zboss_nvram'",
    )
    layout = {**STOCK, "zboss_nvram": (0xF7000, 0x8000)}
    _fails(
        _run(tmp_path, _mcuboot_images(app=layout), MCUBOOT, required=required),
        "overlap",
    )


def test_render_escapes_messages() -> None:
    call = render_check_call(None, [("label", 'a "b"; $c')])
    assert '"label" "a \\"b\\", \\$c"' in call


def test_render_sysbuild_keeps_the_template() -> None:
    text = render_sysbuild_cmake(BootLayout(0x100, ((0x0, 0x100),)), [])
    assert text.startswith("find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})")
    assert "project(sysbuild LANGUAGES)" in text
    assert "BOOT_START 0x100" in text
    assert "RESERVED 0x0 0x100" in text
