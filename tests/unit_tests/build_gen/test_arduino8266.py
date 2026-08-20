"""Drift tests for the native ESP8266 Arduino build generator.

These pin the build spec transliterated from the PlatformIO builder
(framework-arduinoespressif8266/tools/platformio-build.py and
platform-espressif8266/builder/main.py) so a change on either side of the
toolchain seam is caught: the knob-define precedence, the define/flag sets,
the link line, and the core source exclusions must keep matching what the
PlatformIO toolchain produces for the same configuration.
"""

from __future__ import annotations

from pathlib import Path
from unittest.mock import patch

import pytest

from esphome.components.esp8266.boards import BOARDS, ESP8266_BOARD_BUILD
from esphome.components.esp8266.const import (
    KEY_BOARD,
    KEY_ESP8266,
    KEY_FLASH_MODE,
    KEY_SCANF_FLOAT,
)
import esphome.config_validation as cv
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, EsphomeError


@pytest.fixture(autouse=True)
def _setup_core(tmp_path: Path) -> None:
    CORE.name = "test8266"
    CORE.build_path = tmp_path
    CORE.testing_mode = False
    CORE.cpp_standard = "gnu++20"
    CORE.data[KEY_CORE] = {KEY_FRAMEWORK_VERSION: cv.Version(3, 1, 2)}
    CORE.data[KEY_ESP8266] = {
        KEY_BOARD: "nodemcuv2",
        KEY_FLASH_MODE: "dout",
        KEY_SCANF_FLOAT: False,
    }


def _set_flags(*flags: str) -> None:
    CORE.build_flags = set(flags)


def test_board_build_covers_every_board() -> None:
    """Every supported board must have variant/define metadata."""
    assert set(BOARDS) <= set(ESP8266_BOARD_BUILD)


def test_build_config_defaults() -> None:
    from esphome.build_gen.arduino8266 import _flag_defines, _resolve_build_config

    _set_flags()
    config = _resolve_build_config(_flag_defines())
    assert config.nonosdk == "NONOSDK22x_190703"
    assert config.lwip_lib == "lwip2-536-feat"
    assert not config.exceptions
    assert config.vtables == "VTABLES_IN_FLASH"
    assert config.knob_defines == [
        "NONOSDK22x_190703=1",
        "TCP_MSS=536",
        "LWIP_FEATURES=1",
        "LWIP_IPV6=0",
    ]
    assert config.mmu_defines == ["MMU_IRAM_SIZE=0x8000", "MMU_ICACHE_SIZE=0x8000"]


def test_build_config_esphome_lwip_knob() -> None:
    """The lwIP variant ESPHome selects maps to the same defines and library
    as the PlatformIO builder."""
    from esphome.build_gen.arduino8266 import _flag_defines, _resolve_build_config

    _set_flags("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    config = _resolve_build_config(_flag_defines())
    assert config.lwip_lib == "lwip2-1460"
    assert "TCP_MSS=1460" in config.knob_defines
    assert "LWIP_FEATURES=0" in config.knob_defines
    assert "LWIP_IPV6=0" in config.knob_defines


def test_build_config_knobs() -> None:
    from esphome.build_gen.arduino8266 import _flag_defines, _resolve_build_config

    _set_flags(
        "-DPIO_FRAMEWORK_ARDUINO_ESPRESSIF_SDK305",
        "-DPIO_FRAMEWORK_ARDUINO_ENABLE_EXCEPTIONS",
        "-DPIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48",
        "-DVTABLES_IN_DRAM",
    )
    config = _resolve_build_config(_flag_defines())
    assert config.nonosdk == "NONOSDK305"
    assert config.exceptions
    assert config.vtables == "VTABLES_IN_DRAM"
    assert config.mmu_defines == ["MMU_IRAM_SIZE=0xC000", "MMU_ICACHE_SIZE=0x4000"]


def test_build_config_mmu_custom_requires_sizes() -> None:
    from esphome.build_gen.arduino8266 import _flag_defines, _resolve_build_config

    _set_flags("-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM")
    with pytest.raises(EsphomeError, match="MMU_IRAM_SIZE"):
        _resolve_build_config(_flag_defines())

    _set_flags(
        "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM",
        "-DMMU_IRAM_SIZE=0xC000",
        "-DMMU_ICACHE_SIZE=0x4000",
    )
    config = _resolve_build_config(_flag_defines())
    assert sorted(config.mmu_defines) == [
        "MMU_ICACHE_SIZE=0x4000",
        "MMU_IRAM_SIZE=0xC000",
    ]


def test_defines_match_platformio_builder() -> None:
    """The exact define set the PlatformIO builder passes for nodemcuv2/dout."""
    from esphome.build_gen.arduino8266 import (
        _defines_flags,
        _flag_defines,
        _resolve_build_config,
    )

    _set_flags("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    assert _defines_flags(
        _resolve_build_config(_flag_defines()),
        "dout",
        "nodemcuv2",
        ESP8266_BOARD_BUILD["nodemcuv2"]["defines"],
    ) == [
        "-DF_CPU=80000000L",
        "-D__ets__",
        "-DICACHE_FLASH",
        "-D_GNU_SOURCE",
        "-DARDUINO=10805",
        '-DARDUINO_BOARD=\\"PLATFORMIO_NODEMCUV2\\"',
        '-DARDUINO_BOARD_ID=\\"nodemcuv2\\"',
        "-DFLASHMODE_DOUT",
        "-DLWIP_OPEN_SRC",
        "-DNONOSDK22x_190703=1",
        "-DTCP_MSS=1460",
        "-DLWIP_FEATURES=0",
        "-DLWIP_IPV6=0",
        "-DVTABLES_IN_FLASH",
        "-DMMU_IRAM_SIZE=0x8000",
        "-DMMU_ICACHE_SIZE=0x8000",
        "-DESP8266",
        "-DARDUINO_ARCH_ESP8266",
        "-DARDUINO_ESP8266_NODEMCU_ESP12E",
    ]


def _make_framework(tmp_path: Path) -> dict[str, Path]:
    framework = tmp_path / "framework"
    core = framework / "cores" / "esp8266"
    core.mkdir(parents=True)
    for name in (
        "core_esp8266_main.cpp",
        "Updater.cpp",
        "core_esp8266_waveform_pwm.cpp",
        "core_esp8266_waveform_phase.cpp",
        "cont.S",
        "abi.c",
    ):
        (core / name).write_text("")
    (framework / "variants" / "nodemcu").mkdir(parents=True)
    for sub in ("include", "ld", "lwip2/include", "lib"):
        (framework / "tools" / "sdk" / sub).mkdir(parents=True)
    (framework / "libraries").mkdir()
    toolchain = tmp_path / "toolchain"
    (toolchain / "bin").mkdir(parents=True)
    return {
        "framework_path": framework,
        "toolchain_path": toolchain,
        "ninja_path": Path("ninja"),
    }


def _write_ninja(paths: dict[str, Path]) -> str:
    from esphome.build_gen import arduino8266

    src = CORE.relative_src_path()
    (src / "esphome" / "components" / "esp8266").mkdir(parents=True, exist_ok=True)
    (src / "main.cpp").write_text("")
    (src / "esphome" / "vendor.c").write_text("")

    with (
        patch.object(arduino8266, "generate_ld_scripts"),
        patch("esphome.arduino8266.framework.ccache_path", return_value=None),
    ):
        arduino8266.write_project(paths)
    return (CORE.relative_pioenvs_path(CORE.name) / "build.ninja").read_text()


def test_write_project_link_line_and_exclusions(tmp_path: Path) -> None:
    paths = _make_framework(tmp_path)
    _set_flags(
        "-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH",
        "-DUSE_ESP8266_WAVEFORM_STUBS",
        "-Wl,--wrap=millis",
        "-Wl,--wrap=printf",
        "-Wno-nonnull-compare",
    )
    content = _write_ninja(paths)

    # Base link flags from the PlatformIO builder
    for flag in (
        "-Wl,--no-check-sections",
        "-Wl,-static",
        "-Wl,--gc-sections",
        "-Wl,-wrap,system_restart_local",
        "-Wl,-wrap,spi_flash_read",
        "-u app_entry",
        "-u _printf_float",
        "-u _DebugExceptionVector",
        "-u _DoubleExceptionVector",
        "-u _KernelExceptionVector",
        "-u _NMIExceptionVector",
        "-u _UserExceptionVector",
    ):
        assert flag in content
    # ESPHome's link flags and the board linker script
    assert "-Wl,--wrap=millis" in content
    assert "-Wl,--wrap=printf" in content
    assert "-T eagle.flash.4m.ld" in content
    # scanf float disabled: the forced-link flag must not appear
    assert "_scanf_float" not in content
    # System libraries with the selected lwIP variant, in the builder's order
    assert (
        "-lhal -lphy -lpp -lnet80211 -llwip2-1460 -lwpa -lcrypto -lmain -lwps "
        "-lbearssl -lespnow -lsmartconfig -lairkiss -lwpa2 -lstdc++ -lm -lc -lgcc"
        in content
    )
    # Core exclusions: native OTA backend and waveform stubs
    assert "Updater.cpp" not in content
    assert "core_esp8266_waveform_pwm.cpp" not in content
    assert "core_esp8266_waveform_phase.cpp" not in content
    assert "core_esp8266_main.cpp.o" in content
    # Assembly and C sources compile through their own rules
    assert "cont.S.o: asm" in content
    assert "abi.c.o: cc" in content
    # throw_stubs is force-included for ESPHome sources only
    src_lines = [line for line in content.splitlines() if "obj/src/" in line]
    assert any("main.cpp.o: cxx" in line for line in src_lines)
    assert content.count("throw_stubs.h") == len(
        [line for line in content.splitlines() if line.startswith("  flags = ")]
    )


def test_write_project_scanf_float_and_waveform_kept(tmp_path: Path) -> None:
    paths = _make_framework(tmp_path)
    CORE.data[KEY_ESP8266][KEY_SCANF_FLOAT] = True
    _set_flags("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    content = _write_ninja(paths)
    assert "-u _scanf_float" in content
    # Waveform not stubbed out: both implementations stay in the archive
    assert "core_esp8266_waveform_pwm.cpp.o" in content
    assert "core_esp8266_waveform_phase.cpp.o" in content
