"""Native ninja build generator for the ESP8266 Arduino core.

Transliterates the PlatformIO build spec for the Arduino ESP8266 framework
(``framework-arduinoespressif8266/tools/platformio-build.py`` plus
``platform-espressif8266/builder/main.py``) into a ``build.ninja`` under
``.pioenvs/<name>/``. The flag sets, defines, link line, linker-script
generation, and ``elf2bin`` invocation deliberately match what PlatformIO
produces so the binaries stay near-identical between the two toolchains.

The ``PIO_FRAMEWORK_ARDUINO_*`` knob defines (lwIP variant, NONOS SDK
version, MMU layout, exceptions, waveform phase) keep working: they are read
from the build flags with the same precedence as the PlatformIO builder.
"""

from __future__ import annotations

import contextlib
from dataclasses import dataclass
import logging
import os
from pathlib import Path
import subprocess
import sys
from typing import TYPE_CHECKING

from esphome.arduino8266.framework import toolchain_tool
from esphome.build_helpers.ninja import (
    escape as _e,
    quote_path as _q,
    shell_token as _shell_token,
)
from esphome.components.esp8266 import build_surgery
from esphome.components.esp8266.boards import (
    BOARDS,
    ESP8266_BOARD_BUILD,
    ESP8266_LD_SCRIPTS,
)
from esphome.components.esp8266.const import (
    KEY_BOARD,
    KEY_ESP8266,
    KEY_FLASH_MODE,
    KEY_FLASH_SIZE,
    KEY_SCANF_FLOAT,
)
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import (
    get_project_cxx_compile_flags,
    strip_win_long_path_prefix,
)
from esphome.helpers import mkdir_p, write_file_if_changed
from esphome.platformio.library import SOURCE_KIND_FOR_SUFFIX, lex_build_flags

if TYPE_CHECKING:
    from esphome.arduino8266.framework import InstalledPaths

_LOGGER = logging.getLogger(__name__)

# Always excluded from the core build: ESPHome uses its own native OTA
# backend, so the Arduino Updater (and its 228-byte global) never links.
_CORE_EXCLUDE_ALWAYS = {"Updater.cpp"}
# Excluded when no component called require_waveform(); waveform_stubs.cpp
# supplies the stopWaveform()/_stopPWM() stubs digitalWrite needs.
_CORE_EXCLUDE_WAVEFORM = {
    "core_esp8266_waveform_pwm.cpp",
    "core_esp8266_waveform_phase.cpp",
}

# From platformio-build.py. The first entry is the default; with multiple SDK
# knobs set (a pathological config) ties break by table order, since
# upstream's tie-break depends on define order and is not reproducible here.
_NONOSDK_VERSIONS = (
    ("SDK22x_190703", "NONOSDK22x_190703"),
    ("SDK221", "NONOSDK221"),
    ("SDK22x_190313", "NONOSDK22x_190313"),
    ("SDK22x_191024", "NONOSDK22x_191024"),
    ("SDK22x_191105", "NONOSDK22x_191105"),
    ("SDK22x_191122", "NONOSDK22x_191122"),
    ("SDK305", "NONOSDK305"),
)

# knob define -> (TCP_MSS, LWIP_FEATURES, LWIP_IPV6, library name)
_LWIP_VARIANTS = (
    ("PIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY", (536, 1, 1, "lwip6-536-feat")),
    (
        "PIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_HIGHER_BANDWIDTH",
        (1460, 1, 1, "lwip6-1460-feat"),
    ),
    ("PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH", (1460, 1, 0, "lwip2-1460-feat")),
    ("PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY_LOW_FLASH", (536, 0, 0, "lwip2-536")),
    (
        "PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH",
        (1460, 0, 0, "lwip2-1460"),
    ),
)
_LWIP_DEFAULT = (536, 1, 0, "lwip2-536-feat")

# knob define -> MMU_* defines, first match wins (as in platformio-build.py)
_MMU_VARIANTS = (
    (
        "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48",
        ("MMU_IRAM_SIZE=0xC000", "MMU_ICACHE_SIZE=0x4000"),
    ),
    (
        "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48_SECHEAP_SHARED",
        ("MMU_IRAM_SIZE=0xC000", "MMU_ICACHE_SIZE=0x4000", "MMU_IRAM_HEAP"),
    ),
    (
        "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM32_SECHEAP_NOTSHARED",
        (
            "MMU_IRAM_SIZE=0x8000",
            "MMU_ICACHE_SIZE=0x4000",
            "MMU_SEC_HEAP_SIZE=0x4000",
            "MMU_SEC_HEAP=0x40108000",
        ),
    ),
    (
        "PIO_FRAMEWORK_ARDUINO_MMU_EXTERNAL_128K",
        ("MMU_IRAM_SIZE=0x8000", "MMU_ICACHE_SIZE=0x8000", "MMU_EXTERNAL_HEAP=128"),
    ),
    (
        "PIO_FRAMEWORK_ARDUINO_MMU_EXTERNAL_1024K",
        ("MMU_IRAM_SIZE=0x8000", "MMU_ICACHE_SIZE=0x8000", "MMU_EXTERNAL_HEAP=256"),
    ),
)
# Upstream reads these from the board manifest (build.mmu_iram_size etc.);
# no supported board sets them, so the platformio-build.py defaults are
# hardcoded here rather than drift
_MMU_DEFAULT = ("MMU_IRAM_SIZE=0x8000", "MMU_ICACHE_SIZE=0x8000")

# Upstream's CXXFLAGS (-fno-rtti, the -std level, -f(no-)exceptions) and the
# trailing stdc++/m/c/gcc system libs are composed at emission
# (write_project) from CORE.cpp_standard and _BuildConfig.exceptions.
_ASFLAGS = ["-mlongcalls", "-mtext-section-literals"]
_CFLAGS = [
    "-std=gnu17",
    "-Wpointer-arith",
    "-Wno-implicit-function-declaration",
    "-Wl,-EL",
    "-fno-inline-functions",
    "-nostdlib",
]
_CCFLAGS = [
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
# Upstream's -u _scanf_float is deliberately absent: it is re-added from
# KEY_SCANF_FLOAT at emission (the remove_float_scanf extra script's job).
_LINKFLAGS = [
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
_SYSTEM_LIBS_PRE_LWIP = ["hal", "phy", "pp", "net80211"]
_SYSTEM_LIBS_POST_LWIP = [
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


@dataclass
class _BuildConfig:
    """Knob-derived build configuration (PIO_FRAMEWORK_ARDUINO_* defines)."""

    nonosdk: str
    lwip_lib: str
    exceptions: bool
    vtables: str
    fp_in_irom: bool
    knob_defines: list[str]
    mmu_defines: list[str]


def _lexed_build_flags() -> list[str]:
    """Shell-lex ``CORE.build_flags`` as PlatformIO's ``ParseFlags`` does,
    sorted so duplicate defines resolve deterministically.

    Lex once per build; consumers share the tokens.
    """
    return lex_build_flags(sorted(CORE.build_flags), "esphome")


def _flag_defines(unflags: set[str], tokens: list[str]) -> dict[str, str]:
    """Map define name -> full ``NAME[=VALUE]`` for every -D build flag.

    ``tokens`` comes from one ``_lexed_build_flags()`` call shared with
    ``_project_flags`` so a malformed entry warns once, structurally.
    """
    defines: dict[str, str] = {}
    for tok in tokens:
        # An unflagged knob must not drive lwIP/SDK/MMU selection while
        # being absent from the compile line
        if tok in unflags:
            continue
        # A bare "-D" is skipped here and warned about in _project_flags,
        # which sees the same token list
        if tok.startswith("-D") and len(tok) > 2:
            body = tok[2:]
            defines[body.split("=", 1)[0]] = body
    return defines


def _resolve_build_config(defines: dict[str, str]) -> _BuildConfig:
    nonosdk = _NONOSDK_VERSIONS[0][1]
    for name, define in _NONOSDK_VERSIONS:
        if f"PIO_FRAMEWORK_ARDUINO_ESPRESSIF_{name}" in defines:
            nonosdk = define
            break

    tcp_mss, features, ipv6, lwip_lib = _LWIP_DEFAULT
    for knob, variant in _LWIP_VARIANTS:
        if knob in defines:
            tcp_mss, features, ipv6, lwip_lib = variant
            break

    # The lwIP triple selects a prebuilt library; a raw override would win
    # the compile line (user tokens come last here) while the link still
    # pulls the library built for the knob's values
    if owned := sorted(
        n for n in ("TCP_MSS", "LWIP_FEATURES", "LWIP_IPV6") if n in defines
    ):
        raise EsphomeError(
            f"{', '.join(owned)} are set by the PIO_FRAMEWORK_ARDUINO_LWIP2_* "
            "knobs; drop the raw build flags"
        )
    knob_defines = [
        f"{nonosdk}=1",
        f"TCP_MSS={tcp_mss}",
        f"LWIP_FEATURES={features}",
        f"LWIP_IPV6={ipv6}",
    ]
    if "PIO_FRAMEWORK_ARDUINO_WAVEFORM_LOCKED_PHASE" in defines:
        knob_defines.append("WAVEFORM_LOCKED_PHASE=1")

    # Sorted so the pick is deterministic: the dict is built from a set of
    # build flags, whose iteration order varies between processes.
    vtables_knobs = sorted(name for name in defines if name.startswith("VTABLES_IN_"))
    known_vtables = {"VTABLES_IN_FLASH", "VTABLES_IN_DRAM", "VTABLES_IN_IRAM"}
    # A typo'd or conflicting knob would otherwise fail obscurely in the
    # SDK header's #error
    if unknown := [k for k in vtables_knobs if k not in known_vtables]:
        raise EsphomeError(f"Unknown VTABLES_IN_* define(s): {', '.join(unknown)}")
    if len(vtables_knobs) > 1:
        raise EsphomeError(
            f"Conflicting VTABLES_IN_* defines: {', '.join(vtables_knobs)}"
        )
    vtables = vtables_knobs[0] if vtables_knobs else "VTABLES_IN_FLASH"

    mmu_knob = next((knob for knob, _variant in _MMU_VARIANTS if knob in defines), None)
    if mmu_knob is not None:
        if raw := sorted(n for n in defines if n.startswith("MMU_")):
            # Same compile-line/linker-script split as the no-knob case below
            fix = (
                f"drop {mmu_knob} to use the custom sizes"
                if "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM" in defines
                else "drop the raw MMU_* build flags or use "
                "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM"
            )
            raise EsphomeError(f"{', '.join(raw)} conflict with {mmu_knob}; {fix}")
        mmu = list(dict(_MMU_VARIANTS)[mmu_knob])
    elif "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM" in defines:
        if "MMU_IRAM_SIZE" not in defines or "MMU_ICACHE_SIZE" not in defines:
            raise EsphomeError(
                "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM requires MMU_IRAM_SIZE and "
                "MMU_ICACHE_SIZE build flags"
            )
        # Sorted so build.ninja and the linker-script stamp stay
        # byte-stable across runs (the flag set has no deterministic
        # iteration order).
        mmu = sorted(body for name, body in defines.items() if name.startswith("MMU_"))
    else:
        if "MMU_IRAM_SIZE" in defines or "MMU_ICACHE_SIZE" in defines:
            # Unlike PlatformIO (whose defaults win the compile line), user
            # MMU_* here would win the compile but not the linker script; refuse.
            raise EsphomeError(
                "Custom MMU_IRAM_SIZE/MMU_ICACHE_SIZE build flags require "
                "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM"
            )
        mmu = list(_MMU_DEFAULT)

    return _BuildConfig(
        nonosdk=nonosdk,
        lwip_lib=lwip_lib,
        exceptions="PIO_FRAMEWORK_ARDUINO_ENABLE_EXCEPTIONS" in defines,
        vtables=vtables,
        fp_in_irom="FP_IN_IROM" in defines,
        knob_defines=knob_defines,
        mmu_defines=mmu,
    )


_INCOMPLETE_INSTALL = "Arduino toolchain install is incomplete"
_CLEAN_HINT = "run 'esphome clean-all' and retry"


def _active_flash_ld_name(flash_ld_name: str) -> str:
    """The flash linker-script filename the link uses (testing mode renames
    the surgically patched copy)."""
    return f"testing_{flash_ld_name}" if CORE.testing_mode else flash_ld_name


def _flash_ld_name(board: str) -> str:
    """The flash linker script: the board's, or a routed user override.

    Published configs override board_build.ldscript to reserve a
    filesystem region or correct a board's assumed flash size; a bare
    name is required because the script resolves via the -L search path.
    """
    override = _pio_option("board_build.ldscript", "")
    if not override:
        return ESP8266_LD_SCRIPTS[BOARDS[board][KEY_FLASH_SIZE]][1]
    if Path(override).name != override:
        raise EsphomeError(
            f"board_build.ldscript must be a bare script name, got {override!r}"
        )
    return override


def _pio_option(key: str, default: str) -> str:
    """A platformio_options value the native build honors (str-normalized).

    Routed into ``CORE.platformio_options`` by core/config.py under the
    arduino toolchain; a repeated option accumulates as a list, where the
    last value wins like a later platformio.ini line.
    """
    value = CORE.platformio_options.get(key)
    if isinstance(value, list):
        value = value[-1] if value else None
    return default if value is None else str(value)


def _defines_flags(
    config: _BuildConfig, flash_mode: str, board: str, board_defines: tuple[str, ...]
) -> list[str]:
    r"""The framework/board -D tokens for the compile line.

    The returned tokens already carry shell-level escaping (the board
    defines embed ``\"``), so they must be emitted unquoted; wrapping
    them in ``_shell_token`` would deliver literal backslashes to gcc.
    """
    return [
        f"-D{d}"
        for d in (
            # Every supported board ships 80 MHz; board_build.f_cpu overrides
            f"F_CPU={_pio_option('board_build.f_cpu', '80000000L')}",
            "__ets__",
            "ICACHE_FLASH",
            "_GNU_SOURCE",
            "ARDUINO=10805",
            f'ARDUINO_BOARD=\\"PLATFORMIO_{board.upper()}\\"',
            f'ARDUINO_BOARD_ID=\\"{board}\\"',
            f"FLASHMODE_{flash_mode.upper()}",
            "LWIP_OPEN_SRC",
            *config.knob_defines,
            config.vtables,
            *config.mmu_defines,
            "ESP8266",
            "ARDUINO_ARCH_ESP8266",
            *board_defines,
        )
    ]


def _unflag_tokens() -> set[str]:
    """``build_unflags`` entries shell-lexed to tokens, as PlatformIO matches."""
    # Lexed like _lexed_build_flags reads build_flags, so "-D FOO" removes
    # -DFOO in both spellings (PlatformIO's ProcessUnFlags parses the same
    # way) and no bare half can collaterally drop an unrelated token
    return set(lex_build_flags(list(CORE.build_unflags), "esphome build_unflags"))


def _project_flags(
    unflags: set[str], tokens: list[str]
) -> tuple[list[str], list[str], list[Path], list[str]]:
    """Split the ESPHome build flags into compile, linker, -L, and -l lists.

    Plain-form linker flags (``-T``/``-u``/``-Xlinker``) raise: they would be
    inert on the ``-c`` compile line. ``compile_flags``/``link_flags`` come
    back shell-quoted; ``lib_dirs``/``libs`` are raw, quote at emission.
    """
    compile_flags: list[str] = []
    link_flags: list[str] = []
    lib_dirs: list[Path] = []
    libs: list[str] = []
    for tok in tokens:
        if tok in unflags:
            continue
        if tok in ("-I", "-D"):
            # A bare form from '-I ""' would make gcc eat the next flag as
            # its argument (silently, for a nonexistent include dir)
            _LOGGER.warning("Ignoring empty %s in build_flags", tok)
            continue
        if tok.startswith("-Wl,"):
            link_flags.append(_shell_token(tok))
        elif tok.startswith("-L"):
            if len(tok) == 2:
                # Path("") is the CWD; never add it silently
                _LOGGER.warning("Ignoring empty -L in build_flags")
                continue
            lib_dirs.append(Path(tok[2:]))
        elif tok.startswith("-l"):
            if len(tok) == 2:
                _LOGGER.warning("Ignoring empty -l in build_flags")
                continue
            libs.append(tok[2:])
        else:
            if tok in ("-u", "-e", "-s", "-static", "-nostartfiles") or tok.startswith(
                ("-T", "-Xlinker")
            ):
                # Inert on the -c compile line; the firmware would silently
                # lack the requested link behavior
                raise EsphomeError(
                    f"Linker flag {tok} in build_flags is not routed to the "
                    "link line; use the -Wl, form"
                )
            compile_flags.append(_shell_token(tok))
    return compile_flags, link_flags, lib_dirs, libs


def _collect_sources(root: Path, exclude: set[str] = frozenset()) -> list[Path]:
    return sorted(
        p
        for p in root.rglob("*")
        if p.suffix in SOURCE_KIND_FOR_SUFFIX and p.name not in exclude
    )


def _stat_sig(path: Path) -> str:
    """Size and mtime cache-stamp signature for one input file.

    Absent stays deterministic ("missing": the spawn names it); unreadable
    forces a cache miss every run rather than pinning the stamp to a
    constant that can never notice a later edit.
    """
    try:
        st = path.stat()
        return f"{st.st_size}:{st.st_mtime_ns}"
    except FileNotFoundError:
        return "missing"
    except OSError as err:
        _LOGGER.warning(
            "Could not stat %s (%s); regenerating the linker script every "
            "build. Run 'esphome clean-all' to reinstall the framework.",
            path,
            err,
        )
        return f"unreadable:{os.urandom(8).hex()}"


def generate_ld_scripts(
    paths: InstalledPaths, config: _BuildConfig, flash_ld_name: str
) -> None:
    """Generate the common linker script (and testing-mode flash ld copy).

    Runs the same preprocessor invocation as the PlatformIO builder over
    ``eagle.app.v6.common.ld.h``, then applies ESPHome's surgeries: the wifi
    rate-table DRAM relocation, and enlarged memory segments in testing mode.
    """
    framework = paths.framework
    gcc = toolchain_tool(paths.toolchain, "gcc")
    ld_dir = CORE.relative_pioenvs_path(CORE.name, "ld")
    mkdir_p(ld_dir)

    cmd = [str(gcc), "-CC", "-E", "-P", f"-D{config.vtables}"]
    cmd += [f"-D{d}" for d in config.mmu_defines]
    if config.fp_in_irom:
        cmd.append("-DFP_IN_IROM")
    header = framework / "tools" / "sdk" / "ld" / "eagle.app.v6.common.ld.h"
    cmd += [str(header), "-o", "-"]

    # The inputs are the command line (defines + framework version, which is
    # baked into the paths) plus testing mode; skip the preprocessor spawn on
    # incremental builds when nothing changed.
    output = ld_dir / "local.eagle.app.v6.common.ld"
    stamp = ld_dir / ".local.eagle.app.v6.common.ld.stamp"
    # Stamp includes the header/gcc stat (catches in-place re-extraction)
    # and the surgery fingerprint (a build_surgery edit invalidates old
    # build dirs)
    stamp_content = (
        " ".join(cmd)
        + f" testing={CORE.testing_mode}"
        + f" header={_stat_sig(header)}"
        + f" gcc={_stat_sig(gcc)}"
        + f" {build_surgery.surgery_fingerprint()}"
    )

    def _cached_ld_is_valid() -> bool:
        # Any damaged cache regenerates; never abort the build over it
        try:
            if not (
                output.is_file()
                and stamp.is_file()
                and stamp.read_text(encoding="utf-8") == stamp_content
            ):
                return False
            return "SECTIONS" in output.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            return False

    stderr_note = ld_dir / ".local.eagle.app.v6.common.ld.stderr"
    if not _cached_ld_is_valid():
        try:
            result = subprocess.run(
                cmd, capture_output=True, text=True, check=False, close_fds=False
            )
        except OSError as err:
            # A half-extracted or half-deleted toolchain cache reaches here
            raise EsphomeError(
                f"Could not run {gcc}: {err}; run 'esphome clean-all' and retry"
            ) from err
        if result.returncode != 0:
            raise EsphomeError(f"Generating the linker script failed:\n{result.stderr}")
        if result.stderr.strip():
            # Preprocessor warnings on the success path must reach the user
            # on this and every later cached build (see the re-emit below)
            _LOGGER.warning("Linker-script preprocessor: %s", result.stderr.strip())
            stderr_note.write_text(result.stderr.strip(), encoding="utf-8")
        else:
            stderr_note.unlink(missing_ok=True)
        if "SECTIONS" not in result.stdout:
            # A degenerate zero-exit run must not be stamped as a good cache
            raise EsphomeError(
                "Generated linker script is missing its SECTIONS block; "
                "run 'esphome clean-all' and retry"
            )
        try:
            content = build_surgery.relocate_ratetable(result.stdout)
        except RuntimeError as err:
            # The anchor moved in a new core release: a named error, not a
            # traceback, and never a silently unrelocated rate table
            raise EsphomeError(str(err)) from err
        if CORE.testing_mode:
            try:
                content = build_surgery.apply_testing_memory_patches(
                    content, ("iram1_0_seg",)
                )
            except RuntimeError as err:
                # Same changed-linker-script failure class as the ratetable
                raise EsphomeError(str(err)) from err
        write_file_if_changed(output, content)
        stamp.write_text(stamp_content, encoding="utf-8")
    elif stderr_note.is_file():
        # Re-emit cached preprocessor warnings on cache hits; best-effort
        with contextlib.suppress(OSError, UnicodeDecodeError):
            _LOGGER.warning(
                "Linker-script preprocessor: %s",
                stderr_note.read_text(encoding="utf-8"),
            )

    if CORE.testing_mode:
        # A patched copy of the flash ld in the build dir; resolved through
        # the same -L path as the SDK original it shadows.
        flash_ld = framework / "tools" / "sdk" / "ld" / flash_ld_name
        try:
            flash_ld_text = flash_ld.read_text(encoding="utf-8")
        except OSError as err:
            # Same half-extracted-cache hazard as the gcc spawn above
            raise EsphomeError(
                f"Could not read {flash_ld}: {err}; run 'esphome clean-all' and retry"
            ) from err
        try:
            patched_flash_ld = build_surgery.apply_testing_memory_patches(
                flash_ld_text,
                ("dram0_0_seg", "irom0_0_seg"),
            )
        except RuntimeError as err:
            # Same changed-linker-script failure class as the ratetable
            raise EsphomeError(str(err)) from err
        write_file_if_changed(ld_dir / f"testing_{flash_ld_name}", patched_flash_ld)


def _ninja_compile_edges(
    lines: list[str],
    sources: list[Path],
    root: Path,
    group: str,
    flags: str = "",
) -> list[str]:
    """Emit compile edges for ``sources``; return the object paths."""
    objects = []
    for src in sources:
        rel = src.relative_to(root).as_posix()
        obj = f"obj/{group}/{rel}.o"
        escaped_obj = _e(obj)
        lines.append(
            f"build {escaped_obj}: {SOURCE_KIND_FOR_SUFFIX[src.suffix]} {_e(src)}"
        )
        if flags:
            lines.append(f"  flags = {flags}")
        # Escaped once here: the returned paths only ever appear in build
        # statements (archive/link inputs), which use ninja escaping.
        objects.append(escaped_obj)
    return objects


def _common_parent(paths: list[Path]) -> Path:
    return Path(os.path.commonpath([str(p.parent) for p in paths]))


def write_project(paths: InstalledPaths, ccache: str | None) -> bool:
    """Write the ninja build for the current configuration.

    ``ccache`` is the caller's already-resolved binary (None when disabled)
    so one build never pays the runnability probe per consumer. Returns
    True when ``build.ninja`` changed, so the caller can skip work derived
    purely from it (the compile database) on unchanged builds.
    """
    from esphome.arduino.library import resolve_libraries

    framework = paths.framework
    toolchain_bin = paths.toolchain / "bin"
    build_dir = CORE.relative_pioenvs_path(CORE.name)
    mkdir_p(build_dir)

    unflags = _unflag_tokens()
    # Lexed once so a malformed entry warns once, not per consumer
    build_tokens = _lexed_build_flags()
    flag_defines = _flag_defines(unflags, build_tokens)
    config = _resolve_build_config(flag_defines)
    esp8266_data = CORE.data[KEY_ESP8266]
    board = esp8266_data[KEY_BOARD]
    # Backstop; config validation already rejects unknown boards
    if board not in ESP8266_BOARD_BUILD:
        raise EsphomeError(f"Board '{board}' is not supported by the native toolchain")
    board_build = ESP8266_BOARD_BUILD[board]
    flash_ld_name = _flash_ld_name(board)

    generate_ld_scripts(paths, config, flash_ld_name)

    sdk = framework / "tools" / "sdk"
    core_dir = framework / "cores" / "esp8266"
    variant_dir = framework / "variants" / board_build["variant"]
    src_dir = CORE.relative_src_path()

    libraries = resolve_libraries(
        framework,
        pio_platform="espressif8266",
        board_mcu="esp8266",
        cache_key="arduino8266",
    )

    if not src_dir.is_dir():
        # Generated project state, not install state: clean-all would not help
        raise EsphomeError(f"Generated source directory {src_dir} is missing")
    # A missing install directory would otherwise surface as a wall of
    # include errors; failing here names the path instead.
    include_dirs = [
        src_dir,
        sdk / "include",
        core_dir,
        paths.toolchain / "include",
        sdk / "lwip2" / "include",
        variant_dir,
    ]
    for required in include_dirs[1:]:
        if not required.is_dir():
            raise EsphomeError(
                f"{_INCOMPLETE_INSTALL}: missing {required}; {_CLEAN_HINT}"
            )
    # The elf2bin edge runs after the full compile and link; a
    # half-extracted package must fail here, not an hour of wall-clock later
    for required_file in (
        framework / "tools" / "elf2bin.py",
        framework / "bootloaders" / "eboot" / "eboot.elf",
    ):
        if not required_file.is_file():
            raise EsphomeError(
                f"{_INCOMPLETE_INSTALL}: missing {required_file}; {_CLEAN_HINT}"
            )
    for lib in libraries:
        include_dirs += lib.include_dirs

    (
        project_compile_flags,
        project_link_flags,
        project_lib_dirs,
        project_libs,
    ) = _project_flags(unflags, build_tokens)
    defines = _defines_flags(
        config, esp8266_data[KEY_FLASH_MODE], board, board_build["defines"]
    )
    includes = [f"-I{_q(d)}" for d in include_dirs]

    common = _CCFLAGS + defines + includes + project_compile_flags
    cflags = _CFLAGS + common
    cpp_standard = CORE.cpp_standard or "gnu++17"
    cxxflags = (
        ["-fno-rtti", f"-std={cpp_standard}"]
        + ["-fexceptions" if config.exceptions else "-fno-exceptions"]
        + common
        + [_shell_token(f) for f in get_project_cxx_compile_flags()]
    )
    # PlatformIO's ASPPCOM passes only -D/-I user flags to assembly; match
    # it (tokens arrive shell-quoted, hence the lstrip)
    asflags = (
        _ASFLAGS
        + defines
        + includes
        + [f for f in project_compile_flags if f.lstrip("\"'").startswith(("-D", "-I"))]
    )

    # build_unflags applies to the framework flag sets too (compile and link),
    # as under PlatformIO (a silently ignored ``build_unflags: -Os`` would
    # diverge between the toolchains).
    # Matching is whole-token, so an unflag that hits nothing in the user
    # flags or any framework set (a typo, or -DUSE_FOO against -DUSE_FOO=1)
    # must be visible: the user believes the flag is gone while it still
    # drives the compile line and the knob selection
    flag_universe = set(build_tokens)
    for flags in (cflags, cxxflags, asflags, _LINKFLAGS):
        flag_universe.update(flags)
    if unmatched := sorted(unflags - flag_universe):
        _LOGGER.warning(
            "build_unflags entries matched no build flag: %s", ", ".join(unmatched)
        )
    cflags, cxxflags, asflags, link_flags = (
        [f for f in flags if f not in unflags]
        for flags in (cflags, cxxflags, asflags, _LINKFLAGS)
    )
    if esp8266_data[KEY_SCANF_FLOAT]:
        link_flags += ["-u", "_scanf_float"]
    link_flags += project_link_flags
    link_flags += [_shell_token(flag) for lib in libraries for flag in lib.link_flags]
    flash_ld = _active_flash_ld_name(flash_ld_name)
    link_flags += ["-T", flash_ld]

    lib_dirs = [Path("ld"), sdk / "lib", sdk / "ld", sdk / "lib" / config.nonosdk]
    lib_dirs += project_lib_dirs
    for lib in libraries:
        lib_dirs += lib.link_dirs
    system_libs = (
        _SYSTEM_LIBS_PRE_LWIP
        + [config.lwip_lib]
        + _SYSTEM_LIBS_POST_LWIP
        + project_libs
        + [lib_name for lib in libraries for lib_name in lib.link_libs]
        + ["stdc++-exc" if config.exceptions else "stdc++", "m", "c", "gcc"]
    )

    build_tool = Path(__file__).parent / "build_tool.py"

    # $in/$out stay unquoted: ninja escapes its built-in path variables
    # itself; only literal paths need _q().
    lines = [
        "# Auto-generated by ESPHome",
        "ninja_required_version = 1.5",
        f"cc = {_q(toolchain_tool(paths.toolchain, 'gcc'))}",
        f"cxx = {_q(toolchain_tool(paths.toolchain, 'g++'))}",
        # The NSIS launcher starts Python with a \\?\ extended-length path
        # that cmd.exe cannot spawn; same strip every other emitted binary
        # path gets
        f"python = {_q(strip_win_long_path_prefix(sys.executable))}",
        f"buildtool = {_q(build_tool)}",
        f"ccache = {_q(ccache) if ccache else ''}",
        "",
        # Rule names match SOURCE_KIND_FOR_SUFFIX values (c, cxx, asm)
        "rule c",
        "  command = $ccache $cc -MMD -MF $out.d $cflags $flags -c $in -o $out",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = CC $out",
        "rule cxx",
        "  command = $ccache $cxx -MMD -MF $out.d $cxxflags $flags -c $in -o $out",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = CXX $out",
        "rule asm",
        "  command = $ccache $cc -MMD -MF $out.d -x assembler-with-cpp $asflags $flags -c $in -o $out",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = AS $out",
        "rule ar",
        f"  command = $python $buildtool ar {_q(toolchain_tool(paths.toolchain, 'ar'))} $out $out.rsp",
        "  rspfile = $out.rsp",
        "  rspfile_content = $in_newline",
        "  description = AR $out",
        "rule link",
        "  command = $cxx -o $out $linkflags @$out.rsp $libdirflags -Wl,--start-group $archives $libflags -Wl,--end-group",
        "  rspfile = $out.rsp",
        "  rspfile_content = $in_newline",
        "  description = LINK $out",
        "rule elf2bin",
        # --flash_freq 40: every supported board's f_flash is 40 MHz;
        # re-check on a platform bump
        f"  command = $python {_q(framework / 'tools' / 'elf2bin.py')} --eboot {_q(framework / 'bootloaders' / 'eboot' / 'eboot.elf')} --app $in --flash_mode {esp8266_data[KEY_FLASH_MODE]} --flash_freq 40 --flash_size {_flash_size_str(BOARDS[board][KEY_FLASH_SIZE])} --path {_q(toolchain_bin)} --out $out",
        "  description = BIN $out",
        "rule copy",
        "  command = $python $buildtool copy $in $out",
        "  description = COPY $out",
        "",
        f"cflags = {' '.join(cflags)}",
        f"cxxflags = {' '.join(cxxflags)}",
        f"asflags = {' '.join(asflags)}",
        f"linkflags = {' '.join(link_flags)}",
        f"libdirflags = {' '.join(f'-L{_q(d)}' for d in lib_dirs)}",
        f"libflags = {' '.join(_shell_token(f'-l{lib}') for lib in system_libs)}",
        "",
    ]

    core_exclude = set(_CORE_EXCLUDE_ALWAYS)
    if "USE_ESP8266_WAVEFORM_STUBS" in flag_defines:
        core_exclude |= _CORE_EXCLUDE_WAVEFORM

    archives = []
    direct_objs: list[str] = []
    # variant_dir existence was already enforced with the include dirs
    variant_sources = _collect_sources(variant_dir)
    if variant_sources:
        objs = _ninja_compile_edges(lines, variant_sources, variant_dir, "variant")
        lines.append(f"build libFrameworkArduinoVariant.a: ar {' '.join(objs)}")
        archives.append("libFrameworkArduinoVariant.a")

    core_objs = _ninja_compile_edges(
        lines, _collect_sources(core_dir, core_exclude), core_dir, "core"
    )
    if not core_objs:
        # An empty archive would link into a wall of undefined references
        # (app_entry, the exception vectors) far from the cause
        raise EsphomeError(
            f"{_INCOMPLETE_INSTALL}: no core sources in {core_dir}; {_CLEAN_HINT}"
        )
    lines.append(f"build libFrameworkArduino.a: ar {' '.join(core_objs)}")
    archives.append("libFrameworkArduino.a")

    for lib in libraries:
        if not lib.sources:
            # Header-only libraries are legitimate; the log makes an empty
            # srcFilter or broken tree traceable before link errors do.
            _LOGGER.debug(
                "Library %s has no source files; contributing includes only",
                lib.name,
            )
            continue
        lib_root = _common_parent(lib.sources)
        objs = _ninja_compile_edges(
            lines,
            lib.sources,
            lib_root,
            f"lib/{lib.name}",
            flags=" ".join(_shell_token(f) for f in lib.flags),
        )
        if not lib.lib_archive:
            # libArchive: false / dot_a_linkage=false: hand the objects to
            # the linker directly so unreferenced-but-required symbols
            # (exception handlers, weak overrides) survive
            direct_objs.extend(objs)
            continue
        archive = f"lib{lib.name}.a"
        lines.append(f"build {_e(archive)}: ar {' '.join(objs)}")
        archives.append(archive)

    src_extra = f"-include {_q(src_dir / 'esphome' / 'components' / 'esp8266' / 'throw_stubs.h')}"
    # One shared variable instead of repeating the flags line on every src
    # edge (hundreds of edges in a real project)
    lines.append(f"srcflags = {src_extra}")
    src_objs = _ninja_compile_edges(
        lines, _collect_sources(src_dir), src_dir, "src", flags="$srcflags"
    )

    ld_deps = ["ld/local.eagle.app.v6.common.ld"]
    if CORE.testing_mode:
        ld_deps.append(f"ld/{flash_ld}")
    lines.append(
        f"build firmware.elf: link {' '.join(src_objs + direct_objs)} | "
        f"{' '.join(_e(a) for a in archives)} {' '.join(_e(d) for d in ld_deps)}"
    )
    lines.append(f"  archives = {' '.join(_shell_token(a) for a in archives)}")
    lines.append("build firmware.bin: elf2bin firmware.elf")
    lines.append("build firmware.factory.bin: copy firmware.bin")
    lines.append("build firmware.ota.bin: copy firmware.bin")
    lines.append("default firmware.factory.bin firmware.ota.bin")
    lines.append("")

    return write_file_if_changed(build_dir / "build.ninja", "\n".join(lines))


def get_flash_ld_path(build_dir: Path, paths: InstalledPaths) -> Path:
    """The flash linker script the link actually uses (for size reporting).

    Reads the same install the ninja file linked against instead of
    re-resolving the framework version.
    """
    name = _active_flash_ld_name(_flash_ld_name(CORE.data[KEY_ESP8266][KEY_BOARD]))
    if CORE.testing_mode:
        return build_dir / "ld" / name
    return paths.framework / "tools" / "sdk" / "ld" / name


def _flash_size_str(flash_size: int) -> str:
    """Flash size argument for elf2bin (e.g. ``4M``, ``512K``)."""
    mb = 1024 * 1024
    return f"{flash_size // mb}M" if flash_size >= mb else f"{flash_size // 1024}K"
