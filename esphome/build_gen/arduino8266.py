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

from dataclasses import dataclass
import hashlib
import logging
import os
from pathlib import Path
import re
import subprocess
from typing import TYPE_CHECKING

from esphome.arduino8266.framework import toolchain_tool
from esphome.build_helpers.ninja import shell_token as _shell_token
from esphome.components.esp8266 import build_surgery
from esphome.core import CORE, EsphomeError
from esphome.helpers import mkdir_p, write_file_if_changed
from esphome.platformio.library import BARE_ARG_FLAGS, lex_build_flags

if TYPE_CHECKING:
    from esphome.arduino8266.framework import InstalledPaths

_LOGGER = logging.getLogger(__name__)

# Values that land unquoted on generated command lines are shape-checked
# against these before use
_MMU_HEX_VALUE_RE = re.compile(r"0[xX][0-9a-fA-F]+[uUlL]*")
_BOARD_NAME_RE = re.compile(r"[\w.-]+")
_F_CPU_RE = re.compile(r"\d+L?")
_FLASH_LD_NAME_RE = re.compile(r"[\w.-]+\.ld")

# Every supported board ships this clock; board_build.f_cpu overrides
_DEFAULT_F_CPU = "80000000L"

# The SDK linker-script template and the preprocessed copy the build links
# against; the cache stamp and stderr sidecars derive from the output name
_COMMON_LD_HEADER = "eagle.app.v6.common.ld.h"
_COMMON_LD_NAME = "local.eagle.app.v6.common.ld"

# From platformio-build.py. Knob suffix -> SDK define; the first entry is
# the default (dicts preserve insertion order). With multiple SDK knobs set
# (a pathological config) ties break by table order, since upstream's
# tie-break depends on define order and is not reproducible here.
_NONOSDK_VERSIONS = {
    "SDK22x_190703": "NONOSDK22x_190703",
    "SDK221": "NONOSDK221",
    "SDK22x_190313": "NONOSDK22x_190313",
    "SDK22x_191024": "NONOSDK22x_191024",
    "SDK22x_191105": "NONOSDK22x_191105",
    "SDK22x_191122": "NONOSDK22x_191122",
    "SDK305": "NONOSDK305",
}

# Knob define -> (TCP_MSS, LWIP_FEATURES, LWIP_IPV6, library name); first
# match wins, in insertion order (as in platformio-build.py)
_LWIP_VARIANTS = {
    "PIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY": (536, 1, 1, "lwip6-536-feat"),
    "PIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_HIGHER_BANDWIDTH": (
        1460,
        1,
        1,
        "lwip6-1460-feat",
    ),
    "PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH": (1460, 1, 0, "lwip2-1460-feat"),
    "PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY_LOW_FLASH": (536, 0, 0, "lwip2-536"),
    "PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH": (
        1460,
        0,
        0,
        "lwip2-1460",
    ),
}
_LWIP_DEFAULT = (536, 1, 0, "lwip2-536-feat")

# Knob define -> MMU_* defines; first match wins, in insertion order (as
# in platformio-build.py)
_MMU_VARIANTS = {
    "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48": (
        "MMU_IRAM_SIZE=0xC000",
        "MMU_ICACHE_SIZE=0x4000",
    ),
    "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48_SECHEAP_SHARED": (
        "MMU_IRAM_SIZE=0xC000",
        "MMU_ICACHE_SIZE=0x4000",
        "MMU_IRAM_HEAP",
    ),
    "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM32_SECHEAP_NOTSHARED": (
        "MMU_IRAM_SIZE=0x8000",
        "MMU_ICACHE_SIZE=0x4000",
        "MMU_SEC_HEAP_SIZE=0x4000",
        "MMU_SEC_HEAP=0x40108000",
    ),
    "PIO_FRAMEWORK_ARDUINO_MMU_EXTERNAL_128K": (
        "MMU_IRAM_SIZE=0x8000",
        "MMU_ICACHE_SIZE=0x8000",
        "MMU_EXTERNAL_HEAP=128",
    ),
    # Upstream really does cap the 1024K option's heap knob at 256
    # (platformio-build.py's MMU_EXTERNAL_1024K branch); transliterated
    # verbatim
    "PIO_FRAMEWORK_ARDUINO_MMU_EXTERNAL_1024K": (
        "MMU_IRAM_SIZE=0x8000",
        "MMU_ICACHE_SIZE=0x8000",
        "MMU_EXTERNAL_HEAP=256",
    ),
}
# From platformio-build.py: the invariant framework defines every TU gets
# (ARDUINO=10805 encodes the IDE compatibility level); the board, flash-mode,
# knob, and MMU defines are composed around them in _defines_flags, in
# upstream's order.
_FRAMEWORK_DEFINES = ("__ets__", "ICACHE_FLASH", "_GNU_SOURCE", "ARDUINO=10805")
_ARCH_DEFINES = ("ESP8266", "ARDUINO_ARCH_ESP8266")

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
    tokens = lex_build_flags(sorted(CORE.build_flags), "esphome")
    # The lexer glues '-D ""' to a bare "-D"; gcc would eat the next flag
    # as its argument (or add the CWD for -L). Always a typo, so raise for
    # every consumer of the shared token list.
    if empty := sorted({tok for tok in tokens if tok in BARE_ARG_FLAGS}):
        raise EsphomeError(
            f"build_flags contain empty-argument flag(s): {', '.join(empty)}"
        )
    return tokens


def _flag_defines(unflags: set[str], tokens: list[str]) -> dict[str, str]:
    """Map define name -> full ``NAME[=VALUE]`` for every -D build flag.

    ``tokens`` comes from one ``_lexed_build_flags()`` call shared with
    ``_project_flags``, which already raised on any bare "-D".
    """
    defines: dict[str, str] = {}
    for tok in tokens:
        # An unflagged knob must not drive lwIP/SDK/MMU selection while
        # being absent from the compile line
        if tok in unflags:
            continue
        if tok.startswith("-D"):
            body = tok[2:]
            defines[body.split("=", 1)[0]] = body
    return defines


def _resolve_build_config(defines: dict[str, str]) -> _BuildConfig:
    nonosdk = next(iter(_NONOSDK_VERSIONS.values()))
    for name, define in _NONOSDK_VERSIONS.items():
        if f"PIO_FRAMEWORK_ARDUINO_ESPRESSIF_{name}" in defines:
            nonosdk = define
            break
    # Same compile-line/linked-artifact split as the lwIP knobs below: a
    # raw NONOSDK* would define a second SDK macro while the link still
    # resolves against the knob's libraries
    if raw_sdk := sorted(n for n in defines if n.startswith("NONOSDK")):
        raise EsphomeError(
            f"{', '.join(raw_sdk)} are set by the "
            "PIO_FRAMEWORK_ARDUINO_ESPRESSIF_SDK* knobs; drop the raw "
            "build flags"
        )

    tcp_mss, features, ipv6, lwip_lib = _LWIP_DEFAULT
    for knob, variant in _LWIP_VARIANTS.items():
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
    # A body (e.g. VTABLES_IN_FLASH=0) would split the compile line from the
    # linker script, which always defines the bare name
    if valued := [defines[k] for k in vtables_knobs if defines[k] not in (k, f"{k}=1")]:
        raise EsphomeError(f"VTABLES_IN_* defines take no value: {', '.join(valued)}")
    if len(vtables_knobs) > 1:
        raise EsphomeError(
            f"Conflicting VTABLES_IN_* defines: {', '.join(vtables_knobs)}"
        )
    vtables = vtables_knobs[0] if vtables_knobs else "VTABLES_IN_FLASH"

    mmu_knob = next((knob for knob in _MMU_VARIANTS if knob in defines), None)
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
        mmu = list(_MMU_VARIANTS[mmu_knob])
    elif "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM" in defines:
        if "MMU_IRAM_SIZE" not in defines or "MMU_ICACHE_SIZE" not in defines:
            raise EsphomeError(
                "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM requires MMU_IRAM_SIZE and "
                "MMU_ICACHE_SIZE build flags"
            )
        for name, body in defines.items():
            if not name.startswith("MMU_") or "=" not in body:
                # Valueless flags (MMU_IRAM_HEAP) are legitimate switches
                continue
            # Every valued MMU_* reaches the linker-script preprocessor; a
            # bare or non-numeric value would corrupt the segment lengths
            # and fail far away in ld. Hex only: build_surgery's segment
            # parser (and upstream's spellings) cannot read decimal.
            value = body.partition("=")[2]
            if not _MMU_HEX_VALUE_RE.fullmatch(value):
                raise EsphomeError(
                    f"{name} must be a hex literal (e.g. 0x8000), got "
                    f"{value or '(no value)'}"
                )
        # Sorted so build.ninja and the linker-script stamp stay
        # byte-stable across runs (the flag set has no deterministic
        # iteration order).
        mmu = sorted(body for name, body in defines.items() if name.startswith("MMU_"))
    else:
        if raw := sorted(n for n in defines if n.startswith("MMU_")):
            # Unlike PlatformIO (whose defaults win the compile line), user
            # MMU_* here would win the compile but not the linker script;
            # refuse them all, like the knob branch above.
            raise EsphomeError(
                f"Raw {', '.join(raw)} build flags require "
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


def _pio_option(key: str, default: str) -> str:
    """A platformio_options value the native build honors (str-normalized).

    core/config.py routes these into ``CORE.platformio_options`` under the
    arduino toolchain and already collapses a repeated option to its last
    value (like a later platformio.ini line), so a scalar always arrives.
    """
    value = CORE.platformio_options.get(key)
    if value is None:
        return default
    value = str(value).strip()
    if not value:
        raise EsphomeError(f"platformio_options {key} is empty")
    return value


def _defines_flags(
    config: _BuildConfig, flash_mode: str, board: str, board_defines: tuple[str, ...]
) -> list[str]:
    r"""The framework/board -D tokens for the compile line.

    The returned tokens already carry shell-level escaping (the board
    defines embed ``\"``), so they must be emitted unquoted; wrapping
    them in ``_shell_token`` would deliver literal backslashes to gcc.
    """
    if not _BOARD_NAME_RE.fullmatch(board):
        # The name lands unquoted in two -D bodies; reject it by name
        # instead of corrupting the compile line
        raise EsphomeError(f"Invalid board name {board!r}")
    # Every supported board ships 80 MHz; board_build.f_cpu overrides
    f_cpu = _pio_option("board_build.f_cpu", _DEFAULT_F_CPU)
    if not _F_CPU_RE.fullmatch(f_cpu):
        # The value lands unquoted on the compile line; reject by name
        # instead of corrupting it
        raise EsphomeError(f"Invalid board_build.f_cpu value {f_cpu!r}")
    return [
        f"-D{d}"
        for d in (
            f"F_CPU={f_cpu}",
            *_FRAMEWORK_DEFINES,
            f'ARDUINO_BOARD=\\"PLATFORMIO_{board.upper()}\\"',
            f'ARDUINO_BOARD_ID=\\"{board}\\"',
            f"FLASHMODE_{flash_mode.upper()}",
            "LWIP_OPEN_SRC",
            *config.knob_defines,
            config.vtables,
            # User-supplied bodies re-quote like every other user token
            # (a no-op for real MMU values)
            *(_shell_token(d) for d in config.mmu_defines),
            *_ARCH_DEFINES,
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

    Plain-form linker flags (``_PLAIN_LINKER_FLAGS``/``_PLAIN_LINKER_PREFIXES``)
    raise: they would be inert on the ``-c`` compile line.
    ``compile_flags``/``link_flags`` come back shell-quoted;
    ``lib_dirs``/``libs`` are raw, quote at emission.
    """
    compile_flags: list[str] = []
    link_flags: list[str] = []
    lib_dirs: list[Path] = []
    libs: list[str] = []
    for tok in tokens:
        if tok in unflags:
            continue
        # _lexed_build_flags raised on any bare -I/-D/-L/-l
        if tok.startswith("-Wl,"):
            link_flags.append(_shell_token(tok))
        elif tok.startswith("-L"):
            lib_dirs.append(Path(tok[2:]))
        elif tok.startswith("-l"):
            libs.append(tok[2:])
        else:
            if tok in _PLAIN_LINKER_FLAGS or tok.startswith(_PLAIN_LINKER_PREFIXES):
                raise EsphomeError(
                    f"Linker flag {tok} in build_flags is not routed to the "
                    "link line; use the -Wl, form"
                )
            compile_flags.append(_shell_token(tok))
    return compile_flags, link_flags, lib_dirs, libs


# Plain-form linker flags rejected by _project_flags: inert on a -c compile
# line, so the firmware would silently lack the requested link behavior
_PLAIN_LINKER_FLAGS = ("-u", "-e", "-s", "-static", "-nostartfiles")
_PLAIN_LINKER_PREFIXES = ("-T", "-Xlinker")


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


def _write_note(path: Path, text: str, *, warn: bool = False) -> None:
    """Best-effort bookkeeping write; a failure never fails the build.

    ``warn`` marks notes whose loss drops a diagnostic on later cached
    builds; a lost stamp only costs a cache miss and stays at debug.
    """
    try:
        path.write_text(text, encoding="utf-8")
    except OSError as err:
        log = _LOGGER.warning if warn else _LOGGER.debug
        log("Could not write %s: %s", path, err)


def _write_generated(path: Path, content: str) -> None:
    """write_file_if_changed, replacing an unreadable existing copy.

    The recovery is scoped to the comparison read: a damaged cached file is
    logged and overwritten, while a genuine write failure still raises.
    """
    try:
        if path.is_file():
            path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError) as err:
        _LOGGER.warning("Replacing damaged generated file %s: %s", path, err)
        path.unlink(missing_ok=True)
    write_file_if_changed(path, content)


def generate_ld_scripts(
    paths: InstalledPaths, config: _BuildConfig, flash_ld_name: str
) -> None:
    """Generate the common linker script (and testing-mode flash ld copy).

    Runs the same preprocessor invocation as the PlatformIO builder over
    ``eagle.app.v6.common.ld.h``, then applies ESPHome's surgeries: the wifi
    rate-table DRAM relocation, and enlarged memory segments in testing mode.
    """
    if not _FLASH_LD_NAME_RE.fullmatch(flash_ld_name):
        # Joined under the SDK and build ld dirs; never a path or traversal
        raise EsphomeError(f"Invalid flash linker script name {flash_ld_name!r}")
    framework = paths.framework
    gcc = toolchain_tool(paths.toolchain, "gcc")
    ld_dir = CORE.relative_pioenvs_path(CORE.name, "ld")
    mkdir_p(ld_dir)

    cmd = [str(gcc), "-CC", "-E", "-P", f"-D{config.vtables}"]
    cmd += [f"-D{d}" for d in config.mmu_defines]
    if config.fp_in_irom:
        cmd.append("-DFP_IN_IROM")
    header = framework / "tools" / "sdk" / "ld" / _COMMON_LD_HEADER
    cmd += [str(header), "-o", "-"]

    # The inputs are the command line (defines + framework version, which is
    # baked into the paths) plus testing mode; skip the preprocessor spawn on
    # incremental builds when nothing changed.
    output = ld_dir / _COMMON_LD_NAME
    stamp = ld_dir / f".{_COMMON_LD_NAME}.stamp"
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
        # Any damaged cache regenerates; never abort the build over it. The
        # stamp records the sha256 of the content written, so an externally
        # edited script regenerates too.
        try:
            if not (output.is_file() and stamp.is_file()):
                return False
            inputs, sep, digest = stamp.read_text(encoding="utf-8").rpartition(
                " content="
            )
            return (
                bool(sep)
                and inputs == stamp_content
                and hashlib.sha256(output.read_bytes()).hexdigest() == digest
            )
        except (OSError, UnicodeDecodeError):
            return False

    stderr_note = ld_dir / f".{_COMMON_LD_NAME}.stderr"
    if not _cached_ld_is_valid():
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                # Localized gcc diagnostics on a non-UTF-8 console must
                # degrade, not UnicodeDecodeError the build
                encoding="utf-8",
                errors="replace",
                check=False,
                close_fds=False,
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
            _write_note(stderr_note, result.stderr.strip(), warn=True)
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
        _write_generated(output, content)
        _write_note(
            stamp,
            f"{stamp_content} content={hashlib.sha256(content.encode('utf-8')).hexdigest()}",
        )
    elif stderr_note.is_file():
        # Re-emit cached preprocessor warnings on cache hits
        try:
            _LOGGER.warning(
                "Linker-script preprocessor: %s",
                stderr_note.read_text(encoding="utf-8"),
            )
        except (OSError, UnicodeDecodeError):
            _LOGGER.warning(
                "A cached linker-script preprocessor diagnostic exists at %s "
                "but could not be read",
                stderr_note,
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
        _write_generated(ld_dir / f"testing_{flash_ld_name}", patched_flash_ld)
