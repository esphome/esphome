"""Drift tests for the native ESP8266 Arduino build generator.

Pin the transliterated flag/define/link sets against literals audited from
the PlatformIO builder. Upstream drift is caught by the A/B build check on
version bumps, not here.
"""

from __future__ import annotations

from collections.abc import Generator
import logging
import os
from pathlib import Path
import shutil
from unittest.mock import MagicMock, patch

import pytest

from esphome.arduino.library import ArduinoLibrary
from esphome.arduino8266.framework import InstalledPaths, toolchain_tool
from esphome.build_gen import arduino8266
from esphome.build_gen.arduino8266 import (
    _defines_flags,
    _flag_defines,
    _flash_size_str,
    _resolve_build_config,
    get_flash_ld_path,
)
from esphome.components.esp8266.boards import BOARDS, ESP8266_BOARD_BUILD
from esphome.components.esp8266.build_surgery import RATETABLE_RULE
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
def _setup_core(tmp_path: Path) -> Generator[None]:
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
    yield
    # CORE.reset() (the suite-wide autouse fixture) does not clear this flag
    CORE.testing_mode = False


def _set_flags(*flags: str) -> None:
    CORE.build_flags = set(flags)


def _shq(tok: str) -> str:
    """The platform's shell_token quote wrapper (argv rule on Windows)."""
    return f'"{tok}"' if os.name == "nt" else f"'{tok}'"


def _resolve(*flags: str):
    """Set the build flags and resolve the knob config in one step."""
    _set_flags(*flags)
    return _resolve_current()


def _defines():
    """The -D map for the current build flags."""
    return _flag_defines(set(), arduino8266._lexed_build_flags())


def _resolve_current():
    """Resolve whatever flags are already set (must not clear them)."""
    return _resolve_build_config(_defines())


def _split_flags():
    """Classify the current build flags the way write_project does."""
    return arduino8266._project_flags(
        arduino8266._unflag_tokens(), arduino8266._lexed_build_flags()
    )


def _ok_result(stdout=None, stderr=""):
    """A successful preprocessor spawn (defaults to the common ld output)."""
    return MagicMock(
        returncode=0,
        stdout=_COMMON_LD_H_OUTPUT if stdout is None else stdout,
        stderr=stderr,
    )


def test_build_config_defaults() -> None:

    config = _resolve()
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

    config = _resolve("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    assert config.lwip_lib == "lwip2-1460"
    assert "TCP_MSS=1460" in config.knob_defines
    assert "LWIP_FEATURES=0" in config.knob_defines
    assert "LWIP_IPV6=0" in config.knob_defines


def test_build_config_knobs() -> None:

    _set_flags(
        "-DPIO_FRAMEWORK_ARDUINO_ESPRESSIF_SDK305",
        "-DPIO_FRAMEWORK_ARDUINO_ENABLE_EXCEPTIONS",
        "-DPIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48",
        "-DVTABLES_IN_DRAM",
    )
    config = _resolve_current()
    assert config.nonosdk == "NONOSDK305"
    assert config.exceptions
    assert config.vtables == "VTABLES_IN_DRAM"
    assert config.mmu_defines == ["MMU_IRAM_SIZE=0xC000", "MMU_ICACHE_SIZE=0x4000"]


def test_build_config_mmu_custom_requires_sizes() -> None:

    with pytest.raises(EsphomeError, match="MMU_IRAM_SIZE"):
        _resolve("-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM")

    _set_flags(
        "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM",
        "-DMMU_IRAM_SIZE=0xC000",
        "-DMMU_ICACHE_SIZE=0x4000",
    )
    config = _resolve_current()
    # Emitted pre-sorted so build.ninja stays byte-stable across runs
    assert config.mmu_defines == [
        "MMU_ICACHE_SIZE=0x4000",
        "MMU_IRAM_SIZE=0xC000",
    ]


def test_defines_match_platformio_builder() -> None:
    """The exact define set the PlatformIO builder passes for nodemcuv2/dout."""

    _set_flags("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    assert _defines_flags(
        _resolve_current(),
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


def _make_framework(tmp_path: Path) -> InstalledPaths:
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
    (framework / "tools" / "elf2bin.py").write_text("")
    eboot = framework / "bootloaders" / "eboot"
    eboot.mkdir(parents=True)
    (eboot / "eboot.elf").write_text("")
    toolchain = tmp_path / "toolchain"
    (toolchain / "bin").mkdir(parents=True)
    (toolchain / "include").mkdir()
    return InstalledPaths(framework=framework, toolchain=toolchain, ninja=Path("ninja"))


def _write_ninja(
    paths: InstalledPaths,
    libraries: list | None = None,
    ccache: str | None = None,
) -> str:
    src = CORE.relative_src_path()
    (src / "esphome" / "components" / "esp8266").mkdir(parents=True, exist_ok=True)
    (src / "main.cpp").write_text("")
    (src / "esphome" / "vendor.c").write_text("")

    with (
        patch.object(arduino8266, "generate_ld_scripts"),
        patch(
            "esphome.arduino.library.resolve_libraries",
            return_value=libraries or [],
        ),
    ):
        arduino8266.write_project(paths, ccache)
    return (CORE.relative_pioenvs_path(CORE.name) / "build.ninja").read_text()


def test_write_project_link_line_and_exclusions(tmp_path: Path) -> None:
    paths = _make_framework(tmp_path)
    _set_flags(
        "-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH",
        "-DUSE_ESP8266_WAVEFORM_STUBS",
        "-Wl,--wrap=millis",
        "-Wl,--wrap=printf",
        "-Wno-nonnull-compare",
        "-L/opt/blobs",
        "-luser_blob",
        "-L /spc/blobs -l spaced_blob",
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
    # $in/$out must stay unquoted; ninja escapes its own path variables
    assert "-c $in -o $out" in content
    assert "--app $in --flash_mode" in content
    assert '"$in"' not in content
    assert '"$out"' not in content
    # -L/-l from esphome build_flags reach the link line, not the compiles;
    # spaced forms ("-L /path") are shell-lexed the way PlatformIO does.
    # str(Path(...)) so the separator matches the host platform.
    opt_blobs = str(Path("/opt/blobs"))
    spc_blobs = str(Path("/spc/blobs"))
    assert f"-L{_shq(opt_blobs)}" in content
    assert "-luser_blob" in content
    assert f"-L{_shq(spc_blobs)}" in content
    assert "-lspaced_blob" in content
    for line in content.splitlines():
        if line.split(" = ")[0] in ("cflags", "cxxflags", "asflags"):
            assert "user_blob" not in line
            assert opt_blobs not in line
            assert "spaced_blob" not in line
            assert spc_blobs not in line
    # System libraries with the selected lwIP variant, in the builder's order
    assert (
        "-lhal -lphy -lpp -lnet80211 -llwip2-1460 -lwpa -lcrypto -lmain -lwps "
        "-lbearssl -lespnow -lsmartconfig -lairkiss -lwpa2 -lspaced_blob "
        "-luser_blob "
        "-lstdc++ -lm -lc -lgcc" in content
    )
    # Core exclusions: native OTA backend and waveform stubs
    assert "Updater.cpp" not in content
    assert "core_esp8266_waveform_pwm.cpp" not in content
    assert "core_esp8266_waveform_phase.cpp" not in content
    assert "core_esp8266_main.cpp.o" in content
    # Assembly and C sources compile through their own rules
    assert "cont.S.o: asm" in content
    assert "abi.c.o: c" in content
    # throw_stubs is force-included for ESPHome sources only, via one shared
    # srcflags variable rather than a copy of the flags line per edge
    src_lines = [line for line in content.splitlines() if "obj/src/" in line]
    assert any("main.cpp.o: cxx" in line for line in src_lines)
    assert content.count("throw_stubs.h") == 1
    assert "srcflags = -include" in content
    flags_lines = [
        line for line in content.splitlines() if line.startswith("  flags = ")
    ]
    assert flags_lines
    assert all(line == "  flags = $srcflags" for line in flags_lines)


def test_write_project_scanf_float_and_waveform_kept(tmp_path: Path) -> None:
    paths = _make_framework(tmp_path)
    CORE.data[KEY_ESP8266][KEY_SCANF_FLOAT] = True
    _set_flags("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    content = _write_ninja(paths)
    assert "-u _scanf_float" in content
    # Waveform not stubbed out: both implementations stay in the archive
    assert "core_esp8266_waveform_pwm.cpp.o" in content
    assert "core_esp8266_waveform_phase.cpp.o" in content


@pytest.mark.parametrize(
    ("knob", "lib", "mss", "features", "ipv6"),
    [
        ("PIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY", "lwip6-536-feat", 536, 1, 1),
        (
            "PIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_HIGHER_BANDWIDTH",
            "lwip6-1460-feat",
            1460,
            1,
            1,
        ),
        ("PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH", "lwip2-1460-feat", 1460, 1, 0),
        ("PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY_LOW_FLASH", "lwip2-536", 536, 0, 0),
    ],
)
def test_build_config_lwip_variants(
    knob: str, lib: str, mss: int, features: int, ipv6: int
) -> None:
    """Every lwIP knob maps to the same defines and library as the PIO builder."""

    config = _resolve(f"-D{knob}")
    assert config.lwip_lib == lib
    assert f"TCP_MSS={mss}" in config.knob_defines
    assert f"LWIP_FEATURES={features}" in config.knob_defines
    assert f"LWIP_IPV6={ipv6}" in config.knob_defines


@pytest.mark.parametrize(
    ("knob", "expected"),
    [
        (
            "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48_SECHEAP_SHARED",
            ["MMU_IRAM_SIZE=0xC000", "MMU_ICACHE_SIZE=0x4000", "MMU_IRAM_HEAP"],
        ),
        (
            "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM32_SECHEAP_NOTSHARED",
            [
                "MMU_IRAM_SIZE=0x8000",
                "MMU_ICACHE_SIZE=0x4000",
                "MMU_SEC_HEAP_SIZE=0x4000",
                "MMU_SEC_HEAP=0x40108000",
            ],
        ),
        (
            "PIO_FRAMEWORK_ARDUINO_MMU_EXTERNAL_128K",
            ["MMU_IRAM_SIZE=0x8000", "MMU_ICACHE_SIZE=0x8000", "MMU_EXTERNAL_HEAP=128"],
        ),
        (
            "PIO_FRAMEWORK_ARDUINO_MMU_EXTERNAL_1024K",
            ["MMU_IRAM_SIZE=0x8000", "MMU_ICACHE_SIZE=0x8000", "MMU_EXTERNAL_HEAP=256"],
        ),
    ],
)
def test_build_config_mmu_variants(knob: str, expected: list[str]) -> None:

    _set_flags(f"-D{knob}")
    assert _resolve_build_config(_defines()).mmu_defines == expected


def test_build_config_waveform_locked_phase() -> None:

    config = _resolve("-DPIO_FRAMEWORK_ARDUINO_WAVEFORM_LOCKED_PHASE", "-DFP_IN_IROM")
    assert "WAVEFORM_LOCKED_PHASE=1" in config.knob_defines
    assert config.fp_in_irom


_COMMON_LD_H_OUTPUT = """\
MEMORY
{
  iram1_0_seg :    org = 0x40100000, len = 0x8000
}
SECTIONS
{
  .data : ALIGN(4)
  {
    _data_start = ABSOLUTE(.);
  } >dram0_0_seg :dram0_0_phdr
}
"""


def _run_generate_ld_scripts(paths: InstalledPaths) -> Path:

    config = _resolve_current()
    arduino8266.generate_ld_scripts(paths, config, "eagle.flash.4m.ld")
    return CORE.relative_pioenvs_path(CORE.name, "ld")


def test_generate_ld_scripts(tmp_path: Path) -> None:

    paths = _make_framework(tmp_path)
    _set_flags("-DFP_IN_IROM")
    result = _ok_result()
    with (
        patch.object(arduino8266.subprocess, "run", return_value=result) as mock_run,
        patch.object(arduino8266._LOGGER, "warning") as mock_warn,
    ):
        ld_dir = _run_generate_ld_scripts(paths)
    # A clean preprocessor run must be quiet
    mock_warn.assert_not_called()
    content = (ld_dir / "local.eagle.app.v6.common.ld").read_text()
    assert RATETABLE_RULE in content
    cmd = mock_run.call_args[0][0]
    assert "-DVTABLES_IN_FLASH" in cmd
    assert "-DMMU_IRAM_SIZE=0x8000" in cmd
    assert "-DFP_IN_IROM" in cmd

    # Unchanged inputs skip the preprocessor spawn on the next run
    with patch.object(arduino8266.subprocess, "run") as mock_run:
        _run_generate_ld_scripts(paths)
    mock_run.assert_not_called()

    # An edit to the surgery constants invalidates the stamp (a stale linker
    # script would otherwise persist until an esphome clean)
    with (
        patch.object(
            arduino8266.build_surgery, "surgery_fingerprint", return_value="changed"
        ),
        patch.object(arduino8266.subprocess, "run", return_value=result) as mock_run,
    ):
        _run_generate_ld_scripts(paths)
    mock_run.assert_called_once()


def test_generate_ld_scripts_corrupt_cache_regenerates(tmp_path: Path) -> None:
    """A truncated cached linker script regenerates even with a fresh stamp."""
    paths = _make_framework(tmp_path)
    result = _ok_result()
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        ld_dir = _run_generate_ld_scripts(paths)
    output = ld_dir / "local.eagle.app.v6.common.ld"
    output.write_text("truncated garbage")
    with patch.object(arduino8266.subprocess, "run", return_value=result) as mock_run:
        _run_generate_ld_scripts(paths)
    mock_run.assert_called_once()
    assert RATETABLE_RULE in output.read_text()


def test_generate_ld_scripts_failure(tmp_path: Path) -> None:

    paths = _make_framework(tmp_path)
    result = MagicMock(returncode=1, stderr="nope")
    with (
        patch.object(arduino8266.subprocess, "run", return_value=result),
        pytest.raises(EsphomeError, match="linker script failed"),
    ):
        _run_generate_ld_scripts(paths)


def test_generate_ld_scripts_testing_mode(tmp_path: Path) -> None:

    paths = _make_framework(tmp_path)
    (paths.framework / "tools" / "sdk" / "ld" / "eagle.flash.4m.ld").write_text(
        "MEMORY\n{\n"
        "  dram0_0_seg :    org = 0x3FFE8000, len = 0x14000\n"
        "  irom0_0_seg :    org = 0x40201010, len = 0xfeff0\n"
        "}\n"
    )
    CORE.testing_mode = True
    result = _ok_result()
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        ld_dir = _run_generate_ld_scripts(paths)
    patched = (ld_dir / "testing_eagle.flash.4m.ld").read_text()
    assert "len = 0x2000000" in patched


def test_write_project_libraries_and_variant(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:

    paths = _make_framework(tmp_path)
    variant_src = paths.framework / "variants" / "nodemcu" / "variant.cpp"
    variant_src.write_text("")

    lib_dir = tmp_path / "libsrc"
    lib_dir.mkdir()
    (lib_dir / "lib.cpp").write_text("")
    (lib_dir / "impl.cc").write_text("")
    headers_only = ArduinoLibrary(name="HeadersOnly", include_dirs=[lib_dir])
    library = ArduinoLibrary(
        name="MyLib",
        sources=[lib_dir / "impl.cc", lib_dir / "lib.cpp"],
        include_dirs=[lib_dir],
        flags=["-DMYLIB=1"],
        link_dirs=[lib_dir / "blobs"],
        link_libs=["algobsec"],
        link_flags=["-Wl,--wrap=malloc"],
    )
    _set_flags("-DPIO_FRAMEWORK_ARDUINO_ENABLE_EXCEPTIONS")

    with caplog.at_level(logging.DEBUG, logger="esphome.build_gen.arduino8266"):
        content = _write_ninja(
            paths, libraries=[library, headers_only], ccache="/cc/ccache"
        )

    assert "build libFrameworkArduinoVariant.a: ar" in content
    assert "build libMyLib.a: ar" in content
    # A headers-only library contributes includes but no archive, with a
    # debug log distinguishing it from a resolution failure
    assert "libHeadersOnly.a" not in content
    assert "Library HeadersOnly has no source files" in caplog.text
    assert "  flags = -DMYLIB=1" in content
    assert "-lalgobsec" in content
    # Library link flags reach the firmware link line; .cc compiles as C++
    assert "-Wl,--wrap=malloc" in content
    assert "impl.cc.o: cxx" in content
    assert f"-L{_shq(str(lib_dir / 'blobs'))}" in content
    # Exceptions knob: -fexceptions and the exception-enabled stdc++
    assert "-fexceptions" in content
    assert "-lstdc++-exc" in content
    assert f"ccache = {_shq('/cc/ccache')}" in content


def test_get_flash_ld_path(tmp_path: Path) -> None:

    paths = InstalledPaths(
        framework=tmp_path / "framework",
        toolchain=tmp_path / "toolchain",
        ninja=Path("ninja"),
    )
    CORE.testing_mode = True
    assert get_flash_ld_path(tmp_path, paths) == (
        tmp_path / "ld" / "testing_eagle.flash.4m.ld"
    )

    CORE.testing_mode = False
    # Reads the same install the ninja file linked against; no re-resolve
    assert get_flash_ld_path(tmp_path, paths) == (
        tmp_path / "framework" / "tools" / "sdk" / "ld" / "eagle.flash.4m.ld"
    )


def test_flash_size_str() -> None:
    assert _flash_size_str(4 * 1024 * 1024) == "4M"
    assert _flash_size_str(512 * 1024) == "512K"


def test_write_project_testing_mode(tmp_path: Path) -> None:
    paths = _make_framework(tmp_path)
    CORE.testing_mode = True
    _set_flags()
    content = _write_ninja(paths)
    assert "-T testing_eagle.flash.4m.ld" in content
    assert "ld/testing_eagle.flash.4m.ld" in content


def test_write_project_missing_framework_dir_raises(tmp_path: Path) -> None:
    """An incomplete framework install fails naming the missing path."""

    paths = _make_framework(tmp_path)
    shutil.rmtree(paths.framework / "tools" / "sdk" / "lwip2")
    _set_flags()
    with pytest.raises(EsphomeError, match="incomplete.*lwip2"):
        _write_ninja(paths)


def test_generate_ld_scripts_testing_mode_missing_flash_ld_raises(
    tmp_path: Path,
) -> None:
    """A missing flash ld in testing mode names the file and the fix."""
    paths = _make_framework(tmp_path)
    CORE.testing_mode = True
    result = _ok_result()
    with (
        patch.object(arduino8266.subprocess, "run", return_value=result),
        pytest.raises(EsphomeError, match="Could not read .*clean-all"),
    ):
        _run_generate_ld_scripts(paths)


def test_build_config_nonosdk_precedence() -> None:
    """With two SDK knobs set (a pathological config), ties break
    deterministically by table order."""
    _set_flags(
        "-DPIO_FRAMEWORK_ARDUINO_ESPRESSIF_SDK305",
        "-DPIO_FRAMEWORK_ARDUINO_ESPRESSIF_SDK221",
    )
    assert _resolve_build_config(_defines()).nonosdk == "NONOSDK221"


def test_write_project_build_unflags_apply_to_framework_flags(tmp_path: Path) -> None:
    """build_unflags removes flags from the framework sets, as PlatformIO does."""
    paths = _make_framework(tmp_path)
    _set_flags()
    CORE.build_unflags = {"-fipa-pta", "-Wl,--gc-sections"}
    content = _write_ninja(paths)
    for line in content.splitlines():
        key = line.split(" = ")[0]
        if key in ("cflags", "cxxflags", "asflags"):
            assert "-fipa-pta" not in line
        if key == "linkflags":
            assert "-Wl,--gc-sections" not in line


def test_project_flags_trailing_bare_linker_flag_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    _set_flags("-l")
    compile_flags, link_flags, lib_dirs, libs = _split_flags()
    assert "Ignoring trailing '-l'" in caplog.text
    assert not libs
    assert not lib_dirs
    assert "-l" not in compile_flags
    assert "-l" not in link_flags


def test_project_flags_lexed_entry_scatters_non_linker_tokens() -> None:
    _set_flags("-L /d -Wl,-Map=m stray")
    compile_flags, link_flags, lib_dirs, libs = _split_flags()
    assert lib_dirs == [Path("/d")]
    assert link_flags == ["-Wl,-Map=m"]
    assert "stray" in compile_flags
    assert not libs


def test_flag_defines_lexes_multi_token_entries() -> None:
    """A knob inside a multi-token entry is detected like PlatformIO does."""
    _set_flags("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH -DFOO=1 -Os")
    defines = _defines()
    assert "PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH" in defines
    assert defines["FOO"] == "FOO=1"
    config = _resolve_build_config(defines)
    assert config.lwip_lib == "lwip2-1460"


def test_project_flags_lexes_every_entry() -> None:
    """A linker flag anywhere in an entry reaches the link line (PIO parity)."""
    _set_flags("-DFOO=1 -lbar")
    compile_flags, _link, _dirs, libs = _split_flags()
    assert libs == ["bar"]
    assert "-DFOO=1" in compile_flags


def test_project_flags_unflags_match_tokens() -> None:
    """build_unflags removes a token embedded in a multi-token entry."""
    _set_flags("-Os -g3")
    CORE.build_unflags = {"-Os"}
    compile_flags, _link, _dirs, _libs = _split_flags()
    assert "-g3" in compile_flags
    assert "-Os" not in compile_flags


def test_project_flags_requotes_lexed_defines() -> None:
    """A quoted spaced value stays one compiler argument after lex/emit."""
    _set_flags('-DGREETING="hello world"')
    compile_flags, _link, _dirs, _libs = _split_flags()
    # shlex folds the quotes (as PIO's ParseFlags does); _shell_token
    # re-quotes the spaced token so the shell passes one argv element
    assert compile_flags == [_shq("-DGREETING=hello world")]


def test_write_project_empty_core_raises(tmp_path: Path) -> None:
    """A framework tree with no core sources fails at generation, not link."""
    paths = _make_framework(tmp_path)
    core = paths.framework / "cores" / "esp8266"
    for f in core.iterdir():
        f.unlink()
    _set_flags()
    with pytest.raises(EsphomeError, match="no core sources"):
        _write_ninja(paths)


def test_flag_defines_joins_spaced_define() -> None:
    """A spaced "-D KNOB" entry is detected exactly as PlatformIO detects it."""
    _set_flags("-D PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    defines = _defines()
    assert "PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH" in defines
    assert "" not in defines


def test_ninja_path_escaping() -> None:
    """Build-statement paths and command-line paths escape differently."""
    assert arduino8266._e("a b:$c") == "a$ b$:$$c"
    assert arduino8266._q("/a b/$x") == _shq("/a b/$$x")


def test_write_project_asm_excludes_non_define_user_flags(tmp_path: Path) -> None:
    """The ASPPCOM command under PlatformIO never sees CCFLAGS, so only -D/-I user flags
    reach assembly compiles."""
    paths = _make_framework(tmp_path)
    _set_flags("-DUSER_KNOB=1", "-Wno-volatile")
    content = _write_ninja(paths)
    asflags = next(line for line in content.splitlines() if line.startswith("asflags"))
    assert "-DUSER_KNOB=1" in asflags
    assert "-Wno-volatile" not in asflags
    cxxflags = next(
        line for line in content.splitlines() if line.startswith("cxxflags")
    )
    assert "-Wno-volatile" in cxxflags


def test_write_project_returns_changed(tmp_path: Path) -> None:
    """The documented contract: True when build.ninja changed, False on an
    identical regeneration (pins byte-stable output too)."""
    paths = _make_framework(tmp_path)
    _set_flags()
    src = CORE.relative_src_path()
    (src / "esphome" / "components" / "esp8266").mkdir(parents=True, exist_ok=True)
    (src / "main.cpp").write_text("")
    with (
        patch.object(arduino8266, "generate_ld_scripts"),
        patch("esphome.arduino.library.resolve_libraries", return_value=[]),
    ):
        assert arduino8266.write_project(paths, None) is True
        assert arduino8266.write_project(paths, None) is False


def test_write_project_missing_elf2bin_raises(tmp_path: Path) -> None:
    """A half-extracted package must fail by name at generation, not after
    the full compile at the elf2bin edge."""
    paths = _make_framework(tmp_path)
    (paths.framework / "tools" / "elf2bin.py").unlink()
    _set_flags()
    src = CORE.relative_src_path()
    (src / "main.cpp").parent.mkdir(parents=True, exist_ok=True)
    (src / "main.cpp").write_text("")
    with (
        patch.object(arduino8266, "generate_ld_scripts"),
        patch("esphome.arduino.library.resolve_libraries", return_value=[]),
        pytest.raises(EsphomeError, match="elf2bin"),
    ):
        arduino8266.write_project(paths, None)


def test_write_project_missing_src_dir_raises(tmp_path: Path) -> None:
    """A missing generated source tree is its own error, not an install one."""
    paths = _make_framework(tmp_path)
    _set_flags()
    with (
        patch.object(arduino8266, "generate_ld_scripts"),
        patch("esphome.arduino.library.resolve_libraries", return_value=[]),
        patch.object(
            arduino8266.CORE, "relative_src_path", return_value=tmp_path / "nope"
        ),
        pytest.raises(EsphomeError, match="source directory"),
    ):
        arduino8266.write_project(paths, None)


def test_build_config_custom_mmu_without_knob_raises() -> None:
    """Custom MMU sizes without the CUSTOM knob are refused."""
    with pytest.raises(EsphomeError, match="PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM"):
        _resolve("-DMMU_IRAM_SIZE=0xC000")


def test_flag_defines_lexes_quoted_single_tokens() -> None:
    """A quoted single-token define reads the same as on the compile line."""
    _set_flags('-DMMU_SEC_HEAP="0x40108000"')
    assert _defines()["MMU_SEC_HEAP"] == "MMU_SEC_HEAP=0x40108000"


def test_flag_defines_duplicate_defines_resolve_deterministically() -> None:
    """Duplicate conflicting defines pick the same winner every run (sorted
    iteration, last writer wins), independent of the set's hash seed."""
    _set_flags("-DMMU_IRAM_SIZE=0x8000", "-DMMU_IRAM_SIZE=0xC000")
    assert _defines()["MMU_IRAM_SIZE"] == "MMU_IRAM_SIZE=0xC000"


def test_flag_tables_match_platformio_builder() -> None:
    """The transliterated flag lists pinned verbatim, like the define set:
    a drift lands as a test failure, not a binary-size regression."""
    assert arduino8266._ASFLAGS == ["-mlongcalls", "-mtext-section-literals"]
    assert arduino8266._CFLAGS == [
        "-std=gnu17",
        "-Wpointer-arith",
        "-Wno-implicit-function-declaration",
        "-Wl,-EL",
        "-fno-inline-functions",
        "-nostdlib",
    ]
    assert arduino8266._CCFLAGS == [
        "-Os",
        "-mlongcalls",
        "-mtext-section-literals",
        "-falign-functions=4",
        "-U__STRICT_ANSI__",
        "-ffunction-sections",
        "-fdata-sections",
        "-Wall",
        "-Werror=return-type",
        "-free",
        "-fipa-pta",
    ]
    # Pins the deliberate -u _scanf_float omission
    assert arduino8266._LINKFLAGS == [
        "-Os",
        "-nostdlib",
        "-Wl,--no-check-sections",
        "-Wl,-static",
        "-Wl,--gc-sections",
        "-Wl,-wrap,system_restart_local",
        "-Wl,-wrap,spi_flash_read",
        "-u",
        "app_entry",
        "-u",
        "_printf_float",
        "-u",
        "_DebugExceptionVector",
        "-u",
        "_DoubleExceptionVector",
        "-u",
        "_KernelExceptionVector",
        "-u",
        "_NMIExceptionVector",
        "-u",
        "_UserExceptionVector",
    ]
    # Order is load-bearing: upstream's LIBS order resolves symbols correctly
    assert arduino8266._SYSTEM_LIBS_PRE_LWIP == ["hal", "phy", "pp", "net80211"]
    assert arduino8266._SYSTEM_LIBS_POST_LWIP == [
        "wpa",
        "crypto",
        "main",
        "wps",
        "bearssl",
        "espnow",
        "smartconfig",
        "airkiss",
        "wpa2",
    ]


def test_generate_ld_scripts_missing_compiler_is_clean(tmp_path: Path) -> None:
    """A half-deleted toolchain cache fails with an ESPHome error naming the
    binary, not a FileNotFoundError traceback."""
    paths = _make_framework(tmp_path)
    _set_flags()
    with pytest.raises(EsphomeError, match="Could not run"):
        _run_generate_ld_scripts(paths)


def test_write_project_asm_keeps_quoted_defines(tmp_path: Path) -> None:
    """A spaced -D/-I user flag arrives shell-quoted; assembly must still
    receive it."""
    paths = _make_framework(tmp_path)
    _set_flags('-DGREETING="hello world"', "-Wno-volatile")
    content = _write_ninja(paths)
    asflags = next(line for line in content.splitlines() if line.startswith("asflags"))
    assert _shq("-DGREETING=hello world") in asflags
    assert "-Wno-volatile" not in asflags


def test_write_project_unarchived_library_links_objects(tmp_path: Path) -> None:
    """A libArchive:false library's objects reach the link directly."""

    paths = _make_framework(tmp_path)
    lib_src = tmp_path / "gdb" / "src"
    lib_src.mkdir(parents=True)
    (lib_src / "GDBStub.cpp").write_text("")
    _set_flags()
    lib = ArduinoLibrary(
        name="GDBStub",
        sources=[lib_src / "GDBStub.cpp"],
        include_dirs=[lib_src],
        lib_archive=False,
    )
    content = _write_ninja(paths, libraries=[lib])
    assert "libGDBStub.a" not in content
    link_line = next(
        line for line in content.splitlines() if line.startswith("build firmware.elf")
    )
    assert "GDBStub.cpp.o" in link_line


def test_write_project_unknown_board_fails_by_name(tmp_path: Path) -> None:
    """A caller bypassing config validation gets the board named, not a
    KeyError."""
    paths = _make_framework(tmp_path)
    _set_flags()
    CORE.data[KEY_ESP8266][KEY_BOARD] = "not_a_board"
    with pytest.raises(EsphomeError, match="'not_a_board' is not supported"):
        _write_ninja(paths)


def test_unflag_tokens_join_spaced_entries() -> None:
    """Spaced build_unflags entries ("-D FOO") match the joined token."""
    CORE.build_unflags = {"-D FOO", "-l bar"}
    tokens = arduino8266._unflag_tokens()
    assert tokens == {"-DFOO", "-lbar"}
    CORE.build_flags = {"-DFOO -lbar", "-DBAR"}
    compile_flags, _link, _dirs, libs = arduino8266._project_flags(
        tokens, arduino8266._lexed_build_flags()
    )
    assert compile_flags == ["-DBAR"]
    assert libs == []


def test_flag_defines_respects_unflags() -> None:
    """An unflagged knob must not drive the derived toolchain config."""
    _set_flags("-DVTABLES_IN_DRAM")
    defines = _flag_defines({"-DVTABLES_IN_DRAM"}, arduino8266._lexed_build_flags())
    assert "VTABLES_IN_DRAM" not in defines
    config = _resolve_build_config(defines)
    assert config.vtables == "VTABLES_IN_FLASH"


def test_vtables_unknown_raises() -> None:
    """An unknown VTABLES_IN_* knob fails by name."""
    with pytest.raises(EsphomeError, match="Unknown VTABLES_IN_.*BANANA"):
        _resolve("-DVTABLES_IN_BANANA")


def test_vtables_conflicting_raises() -> None:
    with pytest.raises(EsphomeError, match="Conflicting VTABLES_IN_"):
        _resolve("-DVTABLES_IN_DRAM", "-DVTABLES_IN_IRAM")


def test_empty_lib_flags_raise() -> None:
    """A bare -L would silently add the CWD to the search path; the shared
    lex point raises for every consumer."""
    CORE.build_flags = {'-L ""', '-l ""'}
    with pytest.raises(EsphomeError, match=r"empty-argument flag\(s\): -L, -l"):
        arduino8266._lexed_build_flags()


def test_generate_ld_scripts_surfaces_preprocessor_warnings(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """Preprocessor stderr on a zero exit reaches the user; degenerate output is refused."""
    paths = _make_framework(tmp_path)
    _set_flags()
    result = MagicMock(
        returncode=0, stdout=_COMMON_LD_H_OUTPUT, stderr="warning: something"
    )
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        _run_generate_ld_scripts(paths)
    assert "Linker-script preprocessor: warning: something" in caplog.text

    # New flags invalidate the stamp so the degenerate run regenerates
    _set_flags("-DVTABLES_IN_DRAM")
    result = MagicMock(returncode=0, stdout="", stderr="")
    with (
        patch.object(arduino8266.subprocess, "run", return_value=result),
        pytest.raises(EsphomeError, match="SECTIONS"),
    ):
        _run_generate_ld_scripts(paths)


def test_build_config_mmu_knob_with_raw_mmu_flag_raises() -> None:
    """A variant knob plus a raw MMU_* define would split the compile line
    from the linker script; refuse like the no-knob case."""
    with pytest.raises(EsphomeError, match="MMU_IRAM_SIZE conflict with .*CACHE16"):
        _resolve("-DPIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48", "-DMMU_IRAM_SIZE=0x4000")


def test_build_config_raw_lwip_define_raises() -> None:
    """TCP_MSS/LWIP_* belong to the lwIP knobs: a raw value would win the
    compile line while the prebuilt library stays the knob's."""
    with pytest.raises(EsphomeError, match="TCP_MSS are set by the .*LWIP2"):
        _resolve("-DTCP_MSS=1024")


def test_build_config_mmu_defines_do_not_alias_the_table() -> None:
    """The resolved list must be a copy; mutating it must not corrupt the
    module table for later builds in the same process."""
    config = _resolve("-DPIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48")
    config.mmu_defines.append("MMU_BOGUS")
    again = _resolve_current()
    assert "MMU_BOGUS" not in again.mmu_defines
    assert all(isinstance(v, tuple) for v in arduino8266._MMU_VARIANTS.values())


def test_lexed_build_flags_shared_between_consumers(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Lexing once and passing the tokens to both consumers yields the same
    result as each lexing itself, with a malformed entry warned once."""
    _set_flags("-DFOO=1 -l", "-Wl,--wrap=x")
    tokens = arduino8266._lexed_build_flags()
    assert caplog.text.count("Ignoring trailing '-l'") == 1
    assert _flag_defines(set(), tokens) == _flag_defines(
        set(), arduino8266._lexed_build_flags()
    )
    assert arduino8266._project_flags(set(), tokens) == arduino8266._project_flags(
        set(), arduino8266._lexed_build_flags()
    )


@pytest.mark.parametrize(
    "tok", ["-Tcustom.ld", "-Xlinker", "-u", "-e", "-s", "-static", "-nostartfiles"]
)
def test_project_flags_rejects_plain_linker_forms(tok: str) -> None:
    """Plain-form linker flags are refused, naming the -Wl, form."""
    _set_flags(tok)
    with pytest.raises(EsphomeError, match="use the -Wl, form"):
        arduino8266._project_flags(set(), arduino8266._lexed_build_flags())


def test_project_flags_plain_compile_flags_pass() -> None:
    _set_flags("-Os")
    compile_flags, _l, _d, _libs = arduino8266._project_flags(
        set(), arduino8266._lexed_build_flags()
    )
    assert "-Os" in compile_flags


def test_generate_ld_scripts_header_change_invalidates_stamp(
    tmp_path: Path,
) -> None:
    """An in-place framework edit at the same path regenerates the script."""
    paths = _make_framework(tmp_path)
    header = paths.framework / "tools" / "sdk" / "ld" / "eagle.app.v6.common.ld.h"
    header.write_text("v1")
    result = _ok_result()
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        _run_generate_ld_scripts(paths)
    header.write_text("v2 (longer)")
    with patch.object(arduino8266.subprocess, "run", return_value=result) as mock_run:
        _run_generate_ld_scripts(paths)
    mock_run.assert_called_once()


def test_generate_ld_scripts_unreadable_stamp_regenerates(tmp_path: Path) -> None:
    """A non-UTF-8 stamp is a damaged cache: regenerate, never abort."""
    paths = _make_framework(tmp_path)
    result = _ok_result()
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        ld_dir = _run_generate_ld_scripts(paths)
    (ld_dir / ".local.eagle.app.v6.common.ld.stamp").write_bytes(b"\xff\xfe")
    with patch.object(arduino8266.subprocess, "run", return_value=result) as mock_run:
        _run_generate_ld_scripts(paths)
    mock_run.assert_called_once()


def test_vtables_valued_define_raises() -> None:
    """A VTABLES_IN_* body would split the compile line from the linker
    script, which always defines the bare name."""
    with pytest.raises(EsphomeError, match="take no value.*VTABLES_IN_FLASH=0"):
        _resolve("-DVTABLES_IN_FLASH=0")


def test_defines_flags_invalid_board_raises() -> None:
    """The board name lands unquoted in two -D bodies; reject it by name."""
    with pytest.raises(EsphomeError, match="Invalid board name"):
        _defines_flags(_resolve(), "dout", "evil board", ())


def test_generate_ld_scripts_invalid_flash_ld_name_raises(tmp_path: Path) -> None:
    """The script name joins under the SDK and build ld dirs; a traversal
    or path is rejected by name."""
    paths = _make_framework(tmp_path)
    _set_flags()
    config = _resolve()
    with pytest.raises(EsphomeError, match="Invalid flash linker script name"):
        arduino8266.generate_ld_scripts(paths, config, "../evil.ld")


def test_write_generated_replaces_damaged_file(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A non-UTF-8 existing copy is logged and overwritten; the write path
    still raises for real failures (now the shared helper's contract)."""
    from esphome.helpers import write_file_if_changed

    target = tmp_path / "gen.ld"
    target.write_bytes(b"\xff\xfe")
    write_file_if_changed(target, "SECTIONS { }")
    assert target.read_text(encoding="utf-8") == "SECTIONS { }"
    assert "Replacing damaged file" in caplog.text


def test_generate_ld_scripts_edited_output_regenerates(tmp_path: Path) -> None:
    """The stamp records the content hash, so an externally edited cached
    script regenerates instead of linking untrusted content."""
    paths = _make_framework(tmp_path)
    result = _ok_result()
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        ld_dir = _run_generate_ld_scripts(paths)
    output = ld_dir / "local.eagle.app.v6.common.ld"
    output.write_text(output.read_text() + "\n/* tampered */\n")
    with patch.object(arduino8266.subprocess, "run", return_value=result) as mock_run:
        _run_generate_ld_scripts(paths)
    mock_run.assert_called_once()
    assert "tampered" not in output.read_text()


def test_generate_ld_scripts_corrupt_output_is_overwritten(tmp_path: Path) -> None:
    """A non-UTF-8 cached script must be overwritten by the regeneration,
    not abort it (write_file_if_changed reads the old content)."""
    paths = _make_framework(tmp_path)
    result = _ok_result()
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        ld_dir = _run_generate_ld_scripts(paths)
    output = ld_dir / "local.eagle.app.v6.common.ld"
    output.write_bytes(b"\xff\xfe")
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        _run_generate_ld_scripts(paths)
    assert "SECTIONS" in output.read_text(encoding="utf-8")


def test_generate_ld_scripts_unreadable_note_still_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A cached diagnostic that cannot be read must not vanish silently."""
    paths = _make_framework(tmp_path)
    result = _ok_result(stderr="warn!")
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        ld_dir = _run_generate_ld_scripts(paths)
    (ld_dir / ".local.eagle.app.v6.common.ld.stderr").write_bytes(b"\xff\xfe")
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        _run_generate_ld_scripts(paths)
    assert "could not be read" in caplog.text


@pytest.mark.parametrize("value", ["0x8000", "0xC000ul", "0x10UL"])
def test_mmu_custom_numeric_sizes_accepted(value: str) -> None:
    config = _resolve(
        "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM",
        f"-DMMU_IRAM_SIZE={value}",
        "-DMMU_ICACHE_SIZE=0x8000",
    )
    assert f"MMU_IRAM_SIZE={value}" in config.mmu_defines


@pytest.mark.parametrize(
    "flag",
    [
        "-DMMU_IRAM_SIZE=48K",
        # Decimal passes preprocessing but build_surgery's segment parser
        # only reads hex, so testing-mode surgery would fail misleadingly
        "-DMMU_IRAM_SIZE=32768",
    ],
)
def test_mmu_custom_malformed_size_raises(flag: str) -> None:
    """A non-hex size would corrupt the preprocessed segment lengths (or
    defeat the testing-mode surgery); refuse by name."""
    with pytest.raises(EsphomeError, match="MMU_IRAM_SIZE must be a hex"):
        _resolve(
            "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM",
            flag,
            "-DMMU_ICACHE_SIZE=0x8000",
        )


def test_mmu_custom_valueless_switch_accepted_and_others_validated() -> None:
    """Valueless MMU switches (MMU_IRAM_HEAP) pass; every valued MMU_* is
    hex-validated, not just the two required sizes."""
    config = _resolve(
        "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM",
        "-DMMU_IRAM_SIZE=0x8000",
        "-DMMU_ICACHE_SIZE=0x8000",
        "-DMMU_IRAM_HEAP",
    )
    assert "MMU_IRAM_HEAP" in config.mmu_defines
    with pytest.raises(EsphomeError, match="MMU_SEC_HEAP_SIZE must be a numeric"):
        _resolve(
            "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM",
            "-DMMU_IRAM_SIZE=0x8000",
            "-DMMU_ICACHE_SIZE=0x8000",
            "-DMMU_SEC_HEAP_SIZE=48K",
        )


def test_mmu_custom_accepts_decimal_non_segment_values() -> None:
    """MMU_EXTERNAL_HEAP=128 (the module's own EXTERNAL_128K shape) is a
    mmu_iram.h count, not a segment length; decimal is legal there while
    the two segment sizes stay hex-only for the surgery parser."""
    config = _resolve(
        "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM",
        "-DMMU_IRAM_SIZE=0x8000",
        "-DMMU_ICACHE_SIZE=0x8000",
        "-DMMU_EXTERNAL_HEAP=128",
    )
    assert "MMU_EXTERNAL_HEAP=128" in config.mmu_defines


def test_mmu_no_knob_rejects_any_raw_mmu_flag() -> None:
    """The no-knob branch refuses every raw MMU_*, like the knob branch; a
    lone switch would win the compile line but not the linker script."""
    with pytest.raises(EsphomeError, match="Raw MMU_IRAM_HEAP"):
        _resolve("-DMMU_IRAM_HEAP")


def test_raw_nonosdk_define_raises() -> None:
    """A raw NONOSDK* define would split the compile line from the linked
    SDK libraries, like the lwIP knob overrides."""
    with pytest.raises(EsphomeError, match="NONOSDK305 are set by the"):
        _resolve("-DNONOSDK305=1")


def test_write_note_warn_level(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A lost stderr note drops a diagnostic on later cached builds, so it
    warns; a lost stamp only costs a cache miss."""
    arduino8266._write_note(tmp_path / "missing" / "note", "x", warn=True)
    assert "Could not write" in caplog.text


def test_write_note_failure_is_best_effort(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A failed stamp or stderr-note write costs a cache miss, never the
    build."""
    caplog.set_level("DEBUG")
    arduino8266._write_note(tmp_path / "missing" / "stamp", "x")
    assert "Could not write" in caplog.text


def test_pio_option_blank_value_raises() -> None:
    """An empty or blank platformio_options value is a config error, not a
    silent fallback to the default."""
    CORE.platformio_options = {"board_build.f_cpu": "  "}
    with pytest.raises(EsphomeError, match="board_build.f_cpu is empty"):
        arduino8266._pio_option("board_build.f_cpu", "80000000L")


def test_defines_flags_invalid_f_cpu_raises() -> None:
    """A non-numeric board_build.f_cpu is rejected by name; it would land
    unquoted on the compile line."""
    CORE.platformio_options = {"board_build.f_cpu": "160 MHz"}
    with pytest.raises(EsphomeError, match="Invalid board_build.f_cpu"):
        _defines_flags(
            _resolve(),
            "dout",
            "nodemcuv2",
            ESP8266_BOARD_BUILD["nodemcuv2"]["defines"],
        )


def test_generate_ld_scripts_surgery_failure_is_named(tmp_path: Path) -> None:
    """A moved rate-table anchor surfaces as a build error, not a traceback
    or a silently unrelocated table."""
    paths = _make_framework(tmp_path)
    result = MagicMock(returncode=0, stdout="SECTIONS { no anchor here }", stderr="")
    with (
        patch.object(arduino8266.subprocess, "run", return_value=result),
        pytest.raises(EsphomeError, match="anchor not found"),
    ):
        _run_generate_ld_scripts(paths)


def test_write_project_unmatched_unflag_warns(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """An unflag that removes nothing is named; a matching one is silent."""
    paths = _make_framework(tmp_path)
    _set_flags("-DUSE_FOO=1")
    CORE.build_unflags = {"-DUSE_FOO", "-Os"}
    content = _write_ninja(paths)
    assert "matched no build flag: -DUSE_FOO" in caplog.text
    assert "-Os" not in caplog.text.split("matched no build flag")[-1].splitlines()[0]
    # The matching -Os unflag really removed the framework flag
    cflags = next(line for line in content.splitlines() if line.startswith("cflags"))
    assert " -Os " not in cflags


def test_write_project_lexes_build_flags_once(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A malformed build_flags entry warns once per generation."""
    paths = _make_framework(tmp_path)
    _set_flags("-DFOO=1 -l")
    _write_ninja(paths)
    assert caplog.text.count("Ignoring trailing '-l'") == 1


def test_build_config_mmu_conflict_names_the_variant_knob_with_custom() -> None:
    """With MMU_CUSTOM also set, the actionable fix is dropping the variant
    knob, not setting the knob the user already set."""
    with pytest.raises(EsphomeError, match="drop PIO_FRAMEWORK_ARDUINO_MMU_CACHE16"):
        _resolve(
            "-DPIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48",
            "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM",
            "-DMMU_IRAM_SIZE=0xC000",
            "-DMMU_ICACHE_SIZE=0x4000",
        )


def test_generate_ld_scripts_testing_surgery_failure_is_named(
    tmp_path: Path,
) -> None:
    """A testing-mode segment patch failing on a changed linker script is a
    named error, like the ratetable surgery."""
    paths = _make_framework(tmp_path)
    CORE.testing_mode = True
    result = _ok_result()
    with (
        patch.object(arduino8266.subprocess, "run", return_value=result),
        patch.object(
            arduino8266.build_surgery,
            "apply_testing_memory_patches",
            side_effect=RuntimeError("iram1_0_seg not found"),
        ),
        pytest.raises(EsphomeError, match="iram1_0_seg not found"),
    ):
        _run_generate_ld_scripts(paths)


def test_generate_ld_scripts_testing_flash_ld_surgery_failure_is_named(
    tmp_path: Path,
) -> None:
    """The flash-ld segment patch gets the same named-error wrap."""
    paths = _make_framework(tmp_path)
    (paths.framework / "tools" / "sdk" / "ld" / "eagle.flash.4m.ld").write_text(
        "MEMORY { }"
    )
    CORE.testing_mode = True
    result = _ok_result()
    with (
        patch.object(arduino8266.subprocess, "run", return_value=result),
        patch.object(
            arduino8266.build_surgery,
            "apply_testing_memory_patches",
            side_effect=["patched common", RuntimeError("dram0_0_seg mismatch")],
        ),
        pytest.raises(EsphomeError, match="dram0_0_seg mismatch"),
    ):
        _run_generate_ld_scripts(paths)


def test_generate_ld_scripts_reemits_cached_preprocessor_warning(
    tmp_path: Path, caplog: pytest.LogCaptureFixture
) -> None:
    """A preprocessor diagnostic survives cache hits instead of appearing
    once and vanishing for the life of the build dir."""
    paths = _make_framework(tmp_path)
    result = MagicMock(
        returncode=0, stdout=_COMMON_LD_H_OUTPUT, stderr="warning: something odd"
    )
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        _run_generate_ld_scripts(paths)
    assert caplog.text.count("warning: something odd") == 1
    with patch.object(arduino8266.subprocess, "run") as mock_run:
        _run_generate_ld_scripts(paths)
    mock_run.assert_not_called()
    assert caplog.text.count("warning: something odd") == 2


def test_generate_ld_scripts_unreadable_header_forces_regeneration(
    tmp_path: Path,
) -> None:
    """A stat failure other than absence must miss the cache every run, not
    pin the stamp to a constant that can never notice a later edit."""
    paths = _make_framework(tmp_path)
    header_name = "eagle.app.v6.common.ld.h"
    (paths.framework / "tools" / "sdk" / "ld" / header_name).write_text("v1")
    real_stat = Path.stat

    def fake_stat(self: Path, **kwargs: object):
        if self.name == header_name:
            raise PermissionError(13, "denied")
        return real_stat(self, **kwargs)

    result = _ok_result()
    with patch.object(Path, "stat", fake_stat):
        with patch.object(
            arduino8266.subprocess, "run", return_value=result
        ) as mock_run:
            _run_generate_ld_scripts(paths)
        mock_run.assert_called_once()
        with patch.object(
            arduino8266.subprocess, "run", return_value=result
        ) as mock_run:
            _run_generate_ld_scripts(paths)
        mock_run.assert_called_once()


def test_board_tables_are_equal() -> None:
    """BOARDS and ESP8266_BOARD_BUILD must stay exactly in sync."""
    assert set(BOARDS) == set(ESP8266_BOARD_BUILD)


def test_bare_include_and_define_raise() -> None:
    """An empty-argument -I or -D would make gcc eat the next flag as the
    argument; the shared lex point raises for every consumer."""
    CORE.build_flags = {'-I ""', '-D ""'}
    with pytest.raises(EsphomeError, match=r"empty-argument flag\(s\): -D, -I"):
        arduino8266._lexed_build_flags()


def test_generate_ld_scripts_gcc_change_invalidates_stamp(tmp_path: Path) -> None:
    """An in-place toolchain re-extraction regenerates the script, same as
    the header stat."""
    paths = _make_framework(tmp_path)
    gcc = toolchain_tool(paths.toolchain, "gcc")
    gcc.write_text("v1")
    result = _ok_result()
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        _run_generate_ld_scripts(paths)
    gcc.write_text("v2 (longer)")
    with patch.object(arduino8266.subprocess, "run", return_value=result) as mock_run:
        _run_generate_ld_scripts(paths)
    mock_run.assert_called_once()


def test_defines_flags_honors_f_cpu_override() -> None:
    """board_build.f_cpu (a published-config overclock knob) reaches the
    compile line; the default stays 80 MHz."""
    _set_flags()
    config = _resolve_build_config(_flag_defines(set(), []))
    board_build = ESP8266_BOARD_BUILD["nodemcuv2"]
    defines = _defines_flags(config, "dout", "nodemcuv2", board_build["defines"])
    assert "-DF_CPU=80000000L" in defines
    CORE.platformio_options = {"board_build.f_cpu": "160000000L"}
    defines = _defines_flags(config, "dout", "nodemcuv2", board_build["defines"])
    assert "-DF_CPU=160000000L" in defines


def test_flash_ld_name_honors_ldscript_override(tmp_path: Path) -> None:
    """board_build.ldscript (filesystem reservation, corrected flash size)
    replaces the board default; a path is rejected since the name resolves
    via the -L search path."""
    assert arduino8266._flash_ld_name("nodemcuv2") == "eagle.flash.4m.ld"
    CORE.platformio_options = {"board_build.ldscript": "eagle.flash.4m2m.ld"}
    assert arduino8266._flash_ld_name("nodemcuv2") == "eagle.flash.4m2m.ld"
    paths = _make_framework(tmp_path)
    _set_flags()
    content = _write_ninja(paths)
    assert "-T eagle.flash.4m2m.ld" in content
    CORE.platformio_options = {"board_build.ldscript": "../evil.ld"}
    with pytest.raises(EsphomeError, match="bare script name"):
        arduino8266._flash_ld_name("nodemcuv2")


def test_write_project_quotes_spaced_ldscript_override(tmp_path: Path) -> None:
    """An overridden script name re-quotes on the link line like every other
    user token (a space would otherwise split into two argv elements)."""
    CORE.platformio_options = {"board_build.ldscript": "my script.ld"}
    paths = _make_framework(tmp_path)
    _set_flags()
    content = _write_ninja(paths)
    assert "'my script.ld'" in content or '"my script.ld"' in content
    assert "-T my script.ld" not in content
