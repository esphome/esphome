"""Drift tests for the native ESP8266 Arduino build spec.

These pin the build spec transliterated from the PlatformIO builder
(framework-arduinoespressif8266/tools/platformio-build.py and
platform-espressif8266/builder/main.py) so a change on either side of the
toolchain seam is caught: the knob-define precedence, the define/flag sets,
and the linker-script generation must keep matching what the PlatformIO
toolchain produces for the same configuration.
"""

from __future__ import annotations

from collections.abc import Generator
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.arduino8266.framework import InstalledPaths
from esphome.build_gen import arduino8266
from esphome.build_gen.arduino8266 import (
    _defines_flags,
    _flag_defines,
    _resolve_build_config,
)
from esphome.components.esp8266.boards import ESP8266_BOARD_BUILD
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


def test_build_config_defaults() -> None:

    _set_flags()
    config = _resolve_build_config(_flag_defines(set()))
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

    _set_flags("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    config = _resolve_build_config(_flag_defines(set()))
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
    config = _resolve_build_config(_flag_defines(set()))
    assert config.nonosdk == "NONOSDK305"
    assert config.exceptions
    assert config.vtables == "VTABLES_IN_DRAM"
    assert config.mmu_defines == ["MMU_IRAM_SIZE=0xC000", "MMU_ICACHE_SIZE=0x4000"]


def test_build_config_mmu_custom_requires_sizes() -> None:

    _set_flags("-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM")
    with pytest.raises(EsphomeError, match="MMU_IRAM_SIZE"):
        _resolve_build_config(_flag_defines(set()))

    _set_flags(
        "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM",
        "-DMMU_IRAM_SIZE=0xC000",
        "-DMMU_ICACHE_SIZE=0x4000",
    )
    config = _resolve_build_config(_flag_defines(set()))
    # Emitted pre-sorted so build.ninja stays byte-stable across runs
    assert config.mmu_defines == [
        "MMU_ICACHE_SIZE=0x4000",
        "MMU_IRAM_SIZE=0xC000",
    ]


def test_defines_match_platformio_builder() -> None:
    """The exact define set the PlatformIO builder passes for nodemcuv2/dout."""

    _set_flags("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    assert _defines_flags(
        _resolve_build_config(_flag_defines(set())),
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
    (toolchain / "include").mkdir()
    return InstalledPaths(framework=framework, toolchain=toolchain, ninja=Path("ninja"))


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

    _set_flags(f"-D{knob}")
    config = _resolve_build_config(_flag_defines(set()))
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
    assert _resolve_build_config(_flag_defines(set())).mmu_defines == expected


def test_build_config_waveform_locked_phase() -> None:

    _set_flags("-DPIO_FRAMEWORK_ARDUINO_WAVEFORM_LOCKED_PHASE", "-DFP_IN_IROM")
    config = _resolve_build_config(_flag_defines(set()))
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

    config = _resolve_build_config(_flag_defines(set()))
    arduino8266.generate_ld_scripts(paths, config, "eagle.flash.4m.ld")
    return CORE.relative_pioenvs_path(CORE.name, "ld")


def test_generate_ld_scripts(tmp_path: Path) -> None:

    paths = _make_framework(tmp_path)
    _set_flags("-DFP_IN_IROM")
    result = MagicMock(returncode=0, stdout=_COMMON_LD_H_OUTPUT)
    with patch.object(arduino8266.subprocess, "run", return_value=result) as mock_run:
        ld_dir = _run_generate_ld_scripts(paths)
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

    # A surgery edit invalidates the stamp via the fingerprint (a stale
    # linker script would otherwise persist until an esphome clean)
    with (
        patch.object(
            arduino8266.build_surgery, "surgery_fingerprint", return_value="changed"
        ),
        patch.object(arduino8266.subprocess, "run", return_value=result) as mock_run,
    ):
        _run_generate_ld_scripts(paths)
    mock_run.assert_called_once()


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
        # Real flash ld scripts carry no iram1_0_seg (that lives in the
        # generated common ld); the surgery rejects one it was not asked about
        "MEMORY\n{\n"
        "  dram0_0_seg :    org = 0x3FFE8000, len = 0x14000\n"
        "  irom0_0_seg :    org = 0x40201010, len = 0xfeff0\n"
        "}\n"
    )
    CORE.testing_mode = True
    result = MagicMock(returncode=0, stdout=_COMMON_LD_H_OUTPUT)
    with patch.object(arduino8266.subprocess, "run", return_value=result):
        ld_dir = _run_generate_ld_scripts(paths)
    patched = (ld_dir / "testing_eagle.flash.4m.ld").read_text()
    assert "len = 0x2000000" in patched


def test_build_config_nonosdk_precedence() -> None:
    """With two SDK knobs set (a pathological config), ties break
    deterministically by table order."""
    _set_flags(
        "-DPIO_FRAMEWORK_ARDUINO_ESPRESSIF_SDK305",
        "-DPIO_FRAMEWORK_ARDUINO_ESPRESSIF_SDK221",
    )
    assert _resolve_build_config(_flag_defines(set())).nonosdk == "NONOSDK221"


def test_project_flags_trailing_bare_linker_flag_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    _set_flags("-l")
    compile_flags, link_flags, lib_dirs, libs = arduino8266._project_flags(
        arduino8266._unflag_tokens()
    )
    assert "Ignoring trailing '-l'" in caplog.text
    assert not libs
    assert not lib_dirs
    assert "-l" not in compile_flags
    assert "-l" not in link_flags


def test_project_flags_lexed_entry_scatters_non_linker_tokens() -> None:
    _set_flags("-L /d -Wl,-Map=m stray")
    compile_flags, link_flags, lib_dirs, libs = arduino8266._project_flags(
        arduino8266._unflag_tokens()
    )
    assert lib_dirs == [Path("/d")]
    assert link_flags == ["-Wl,-Map=m"]
    assert "stray" in compile_flags
    assert not libs


def test_flag_defines_lexes_multi_token_entries() -> None:
    """A knob inside a multi-token entry is detected like PlatformIO does."""
    _set_flags("-DPIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH -DFOO=1 -Os")
    defines = _flag_defines(set())
    assert "PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH" in defines
    assert defines["FOO"] == "FOO=1"
    config = _resolve_build_config(defines)
    assert config.lwip_lib == "lwip2-1460"


def test_project_flags_lexes_every_entry() -> None:
    """A linker flag anywhere in an entry reaches the link line (PIO parity)."""
    _set_flags("-DFOO=1 -lbar")
    compile_flags, _link, _dirs, libs = arduino8266._project_flags(
        arduino8266._unflag_tokens()
    )
    assert libs == ["bar"]
    assert "-DFOO=1" in compile_flags


def test_project_flags_unflags_match_tokens() -> None:
    """build_unflags removes a token embedded in a multi-token entry."""
    _set_flags("-Os -g3")
    CORE.build_unflags = {"-Os"}
    compile_flags, _link, _dirs, _libs = arduino8266._project_flags(
        arduino8266._unflag_tokens()
    )
    assert "-g3" in compile_flags
    assert "-Os" not in compile_flags


def test_project_flags_requotes_lexed_defines() -> None:
    """A quoted spaced value stays one compiler argument after lex/emit."""
    _set_flags('-DGREETING="hello world"')
    compile_flags, _link, _dirs, _libs = arduino8266._project_flags(
        arduino8266._unflag_tokens()
    )
    # shlex folds the quotes (as PIO's ParseFlags does); _shell_token
    # re-quotes the spaced token so the shell passes one argv element
    assert compile_flags == ['"-DGREETING=hello world"']


def test_flag_defines_joins_spaced_define() -> None:
    """A spaced "-D KNOB" entry is detected exactly as PlatformIO detects it."""
    _set_flags("-D PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH")
    defines = _flag_defines(set())
    assert "PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH" in defines
    assert "" not in defines


def test_build_config_custom_mmu_without_knob_warns(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """Custom MMU sizes without the CUSTOM knob keep the default layout and
    warn, as the PlatformIO builder does."""
    _set_flags("-DMMU_IRAM_SIZE=0xC000")
    config = _resolve_build_config(_flag_defines(set()))
    assert config.mmu_defines == ["MMU_IRAM_SIZE=0x8000", "MMU_ICACHE_SIZE=0x8000"]
    assert "Detected custom MMU flags" in caplog.text


def test_flag_defines_lexes_quoted_single_tokens() -> None:
    """A quoted single-token define reads the same as on the compile line."""
    _set_flags('-DMMU_SEC_HEAP="0x40108000"')
    assert _flag_defines(set())["MMU_SEC_HEAP"] == "MMU_SEC_HEAP=0x40108000"


def test_flag_defines_duplicate_defines_resolve_deterministically() -> None:
    """Duplicate conflicting defines pick the same winner every run (sorted
    iteration, last writer wins), independent of the set's hash seed."""
    _set_flags("-DMMU_IRAM_SIZE=0x8000", "-DMMU_IRAM_SIZE=0xC000")
    assert _flag_defines(set())["MMU_IRAM_SIZE"] == "MMU_IRAM_SIZE=0xC000"


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
    # The -u block is where the deliberate -u _scanf_float omission lives;
    # pinned in full so "restoring" it fails here first
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


def test_unflag_tokens_join_spaced_entries() -> None:
    """A spaced build_unflags entry ("-D FOO") removes -DFOO, and no bare half leaks into the
    unflag set to collaterally drop unrelated tokens."""
    CORE.build_unflags = {"-D FOO", "-l bar"}
    tokens = arduino8266._unflag_tokens()
    assert tokens == {"-DFOO", "-lbar"}
    CORE.build_flags = {"-DFOO -lbar", "-DBAR"}
    compile_flags, _link, _dirs, libs = arduino8266._project_flags(tokens)
    assert compile_flags == ["-DBAR"]
    assert libs == []


def test_flag_defines_respects_unflags() -> None:
    """An unflagged knob must not drive the derived toolchain config."""
    _set_flags("-DVTABLES_IN_DRAM")
    defines = _flag_defines({"-DVTABLES_IN_DRAM"})
    assert "VTABLES_IN_DRAM" not in defines
    config = _resolve_build_config(defines)
    assert config.vtables == "VTABLES_IN_FLASH"


def test_vtables_unknown_and_conflicting_warn(
    caplog: pytest.LogCaptureFixture,
) -> None:
    _set_flags("-DVTABLES_IN_BANANA", "-DVTABLES_IN_DRAM")
    config = _resolve_build_config(_flag_defines(set()))
    assert "Unknown VTABLES_IN_*" in caplog.text
    assert "Multiple VTABLES_IN_*" in caplog.text
    # Deterministic pick, as before
    assert config.vtables == "VTABLES_IN_BANANA"


def test_project_flags_empty_lib_flags_warn(
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A bare -L must not silently add the CWD to the search path."""
    CORE.build_flags = {'-L ""', '-l ""'}
    _c, _l, lib_dirs, libs = arduino8266._project_flags(set())
    assert lib_dirs == []
    assert libs == []
    assert "Ignoring empty -L" in caplog.text
    assert "Ignoring empty -l" in caplog.text


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
