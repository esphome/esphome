"""Build specification for the native ESP8266 Arduino toolchain.

Transliterates the PlatformIO build spec for the Arduino ESP8266 framework
(``framework-arduinoespressif8266/tools/platformio-build.py`` plus
``platform-espressif8266/builder/main.py``): the flag sets, defines, and
linker-script generation deliberately match what PlatformIO produces so the
binaries stay near-identical between the two toolchains. The ninja emission
(``write_project``) builds on these pieces.

The ``PIO_FRAMEWORK_ARDUINO_*`` knob defines (lwIP variant, NONOS SDK
version, MMU layout, exceptions, waveform phase) keep working: they are read
from the build flags with the same precedence as the PlatformIO builder.
"""

from __future__ import annotations

import contextlib
from dataclasses import dataclass, field
import logging
import os
from pathlib import Path
import subprocess
from typing import TYPE_CHECKING

from esphome.build_helpers.ninja import shell_token as _shell_token
from esphome.components.esp8266 import build_surgery
from esphome.core import CORE, EsphomeError
from esphome.helpers import mkdir_p, write_file_if_changed
from esphome.platformio.library import lex_build_flags

if TYPE_CHECKING:
    from esphome.arduino8266.framework import InstalledPaths

_LOGGER = logging.getLogger(__name__)

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
    knob_defines: list[str] = field(default_factory=list)
    mmu_defines: list[str] = field(default_factory=list)


def _lexed_build_flags() -> list[str]:
    """Shell-lex every ``CORE.build_flags`` entry the way PlatformIO's
    ``ParseFlags`` does, so a knob in ``"-DKNOB -DOTHER"``, a spaced
    ``"-D KNOB"``, and quoted bodies all read identically everywhere.

    Sorted so duplicate defines resolve the same way every run (the winner
    feeds the linker-script preprocessor line, which is also the cache
    stamp). Lex once per build and pass the result to ``_flag_defines`` and
    ``_project_flags`` so a malformed entry warns once, not per consumer.
    """
    return lex_build_flags(sorted(CORE.build_flags), "esphome")


def _flag_defines(unflags: set[str], tokens: list[str] | None = None) -> dict[str, str]:
    """Map define name -> full ``NAME[=VALUE]`` for every -D build flag."""
    defines: dict[str, str] = {}
    for tok in _lexed_build_flags() if tokens is None else tokens:
        # An unflagged knob must not drive lwIP/SDK/MMU selection while
        # being absent from the compile line
        if tok in unflags:
            continue
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
    # A typo would otherwise win the sorted pick and end in the SDK header's
    # #error, and a conflicting pair would resolve arbitrarily; both are
    # config errors, not build-time surprises
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
            # PlatformIO only warns here and appends its defaults last so
            # they win the compile line; in this generator the user's
            # tokens would come last instead, compiling against a memory
            # layout the linker script does not implement. Refuse rather
            # than reproduce the upstream footgun with worse odds.
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


def _defines_flags(
    config: _BuildConfig, flash_mode: str, board: str, board_defines: tuple[str, ...]
) -> list[str]:
    return [
        f"-D{d}"
        for d in (
            "F_CPU=80000000L",
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
    unflags: set[str], tokens: list[str] | None = None
) -> tuple[list[str], list[str], list[Path], list[str]]:
    """Split the ESPHome build flags into compile, linker, -L, and -l lists.

    Every entry is shell-lexed the way PlatformIO's ``ParseFlags`` does, so a
    linker flag anywhere in an entry reaches the link line and
    ``build_unflags`` matches individual tokens (``-Os`` inside ``-Os -g3``).
    Only the flag forms ESPHome emits are classified (``-Wl,``/``-L``/``-l``
    and compile flags); plain-form ``-T``/``-u``/``-Xlinker`` raise (inert on
    a ``-c`` compile line), unlike full ParseFlags which routes them to the
    link line. The returned ``compile_flags`` and ``link_flags`` are already
    ``_shell_token``-quoted; ``lib_dirs`` and ``libs`` are raw and the caller
    must quote them at emission.
    """
    compile_flags: list[str] = []
    link_flags: list[str] = []
    lib_dirs: list[Path] = []
    libs: list[str] = []
    for tok in _lexed_build_flags() if tokens is None else tokens:
        if tok in unflags:
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
            if tok == "-u" or tok.startswith(("-T", "-Xlinker")):
                # Inert on the -c compile line; the firmware would silently
                # lack the requested link behavior
                raise EsphomeError(
                    f"Linker flag {tok} in build_flags is not routed to the "
                    "link line; use the -Wl, form"
                )
            compile_flags.append(_shell_token(tok))
    return compile_flags, link_flags, lib_dirs, libs


def generate_ld_scripts(
    paths: InstalledPaths, config: _BuildConfig, flash_ld_name: str
) -> None:
    """Generate the common linker script (and testing-mode flash ld copy).

    Runs the same preprocessor invocation as the PlatformIO builder over
    ``eagle.app.v6.common.ld.h``, then applies ESPHome's surgeries: the wifi
    rate-table DRAM relocation, and enlarged memory segments in testing mode.
    """
    framework = paths.framework
    gcc = paths.toolchain / "bin" / "xtensa-lx106-elf-gcc"
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
    # The surgery constants are inputs too: an edit to build_surgery.py must
    # invalidate existing build dirs, not wait for an esphome clean.
    # The header's size and mtime cover an in-place framework edit or
    # re-extraction at the same versioned path, which the command line
    # alone would not notice
    try:
        header_stat = header.stat()
        header_sig = f"{header_stat.st_size}:{header_stat.st_mtime_ns}"
    except FileNotFoundError:
        header_sig = "missing"  # the preprocessor spawn below names it
    except OSError as err:
        # An unreadable header must force a cache miss every run, not pin
        # the stamp to a constant that can never notice a later edit; say
        # why every build regenerates the script
        _LOGGER.warning(
            "Could not stat %s (%s); regenerating the linker script every "
            "build. Run 'esphome clean-all' to reinstall the framework.",
            header,
            err,
        )
        header_sig = f"unreadable:{os.urandom(8).hex()}"
    stamp_content = (
        " ".join(cmd)
        + f" testing={CORE.testing_mode}"
        + f" header={header_sig}"
        # One fingerprint instead of enumerating surgery internals here, so
        # any behavioral edit in build_surgery self-invalidates the cache
        + f" {build_surgery.surgery_fingerprint()}"
    )

    def _cached_ld_is_valid() -> bool:
        # A damaged cache (unreadable, non-UTF-8, truncated, externally
        # edited) must regenerate, not abort the build or be reused on
        # existence alone (the SECTIONS check below only guards generation)
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
        # The diagnostic must not vanish for the life of the build dir just
        # because the script is cached; best-effort like every other cache
        # read here (a damaged note must not abort an incremental build)
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
