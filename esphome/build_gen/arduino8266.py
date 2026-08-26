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

from collections.abc import Collection
from dataclasses import dataclass
import hashlib
import logging
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
from typing import TYPE_CHECKING, NamedTuple

from esphome.arduino8266.framework import toolchain_tool
from esphome.build_helpers.ccache import effective_ccache_basedir
from esphome.build_helpers.ninja import (
    escape as _e,
    quote_path as _q,
    shell_token as _shell_token,
)
from esphome.build_helpers.pch import (
    PCH_CORE_HEADER,
    PCH_HEADER_NAME,
    pch_checksum,
    pch_enabled,
    pch_header_text,
)
from esphome.components.esp8266 import build_surgery
from esphome.components.esp8266.boards import (
    BOARDS,
    ESP8266_BOARD_BUILD,
    board_ld_script,
)
from esphome.components.esp8266.const import (
    BUILD_FLASH_MODES,
    KEY_BOARD,
    KEY_ESP8266,
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

# Values that land unquoted on generated command lines are shape-checked
# against these before use. re.ASCII: a Unicode digit or word character
# (Arabic-Indic numerals) would pass \d/\w and defeat the named error
_MMU_VALUE_RE = re.compile(r"(?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*", re.ASCII)
_MMU_HEX_VALUE_RE = re.compile(r"0[xX][0-9a-fA-F]+[uUlL]*", re.ASCII)
# Only these land in the preprocessed script's ``len =`` fields, which
# build_surgery's segment parser reads back as hex; the other MMU_* macros
# (MMU_EXTERNAL_HEAP=128) are consumed by mmu_iram.h and may be decimal
_MMU_SEGMENT_SIZE_NAMES = ("MMU_IRAM_SIZE", "MMU_ICACHE_SIZE")
_BOARD_NAME_RE = re.compile(r"[\w.-]+", re.ASCII)
_F_CPU_RE = re.compile(r"\d+L?", re.ASCII)
_FLASH_LD_NAME_RE = re.compile(r"[\w.-]+\.ld", re.ASCII)

# Every supported board ships this clock; board_build.f_cpu overrides
_DEFAULT_F_CPU = "80000000L"

# The SDK linker-script template and the preprocessed copy the build links
# against; the cache stamp and stderr sidecars derive from the output name
_COMMON_LD_HEADER = "eagle.app.v6.common.ld.h"
_COMMON_LD_NAME = "local.eagle.app.v6.common.ld"
# Testing mode shadows the SDK flash ld with a patched copy under this name
_TESTING_LD_PREFIX = "testing_"

# The recovery hint for a half-extracted or damaged framework cache
_CLEAN_HINT = "run 'esphome clean-all' and retry"


def _sdk_ld_dir(framework: Path) -> Path:
    return framework / "tools" / "sdk" / "ld"


def _apply_surgery(fn, *args: object) -> str:
    """Run one build_surgery edit, naming a failed anchor instead of a
    traceback (the surgery module raises bare RuntimeError so its
    ``.py.script`` twins stay importable without esphome)."""
    try:
        return fn(*args)
    except RuntimeError as err:
        raise EsphomeError(str(err)) from err


# Every supported board's f_flash is 40 MHz; re-check on a platform bump
# board_flash_mode's closed set, shared with cv.one_of's validation
_FLASH_MODES = frozenset(BUILD_FLASH_MODES)
_FLASH_FREQ_MHZ = 40

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


class _LwipVariant(NamedTuple):
    """One lwIP build variant: the defines and the prebuilt library that
    was compiled with them."""

    tcp_mss: int
    features: int
    ipv6: int
    lib: str


# Knob define -> variant; first match wins, in insertion order (as in
# platformio-build.py)
_LWIP_VARIANTS = {
    "PIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_LOW_MEMORY": _LwipVariant(
        536, 1, 1, "lwip6-536-feat"
    ),
    "PIO_FRAMEWORK_ARDUINO_LWIP2_IPV6_HIGHER_BANDWIDTH": _LwipVariant(
        1460, 1, 1, "lwip6-1460-feat"
    ),
    "PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH": _LwipVariant(
        1460, 1, 0, "lwip2-1460-feat"
    ),
    "PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY_LOW_FLASH": _LwipVariant(
        536, 0, 0, "lwip2-536"
    ),
    "PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH_LOW_FLASH": _LwipVariant(
        1460, 0, 0, "lwip2-1460"
    ),
}
# The default is PIO_FRAMEWORK_ARDUINO_LWIP2_LOW_MEMORY's variant: upstream
# has no branch for that spelling (it is the else), so any listed knob wins
# over it -- sntp emits LOW_MEMORY while esp8266 always emits
# HIGHER_BANDWIDTH_LOW_FLASH, and the latter must win as under PlatformIO
_LWIP_DEFAULT = _LwipVariant(536, 1, 0, "lwip2-536-feat")

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
    # The funnel warns and drops empty glued arguments (-D "") itself
    return lex_build_flags(sorted(CORE.build_flags), "esphome")


def _flag_defines(unflags: set[str], tokens: list[str]) -> dict[str, str]:
    """Map define name -> full ``NAME[=VALUE]`` for every -D build flag.

    ``tokens`` comes from one ``_lexed_build_flags()`` call shared with
    ``_project_flags``, which already warned about and dropped any bare "-D".
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
    nonosdk = next(
        (
            define
            for name, define in _NONOSDK_VERSIONS.items()
            if f"PIO_FRAMEWORK_ARDUINO_ESPRESSIF_{name}" in defines
        ),
        next(iter(_NONOSDK_VERSIONS.values())),
    )
    # Same compile-line/linked-artifact split as the lwIP knobs below: a
    # raw NONOSDK* would define a second SDK macro while the link still
    # resolves against the knob's libraries
    if raw_sdk := sorted(n for n in defines if n.startswith("NONOSDK")):
        raise EsphomeError(
            f"{', '.join(raw_sdk)} are set by the "
            "PIO_FRAMEWORK_ARDUINO_ESPRESSIF_SDK* knobs; drop the raw "
            "build flags"
        )

    lwip = next(
        (variant for knob, variant in _LWIP_VARIANTS.items() if knob in defines),
        _LWIP_DEFAULT,
    )

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
        f"TCP_MSS={lwip.tcp_mss}",
        f"LWIP_FEATURES={lwip.features}",
        f"LWIP_IPV6={lwip.ipv6}",
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
        for name in _MMU_SEGMENT_SIZE_NAMES:
            # A bare -Dname would preprocess to len = 1 and fail far away
            if "=" not in defines[name]:
                raise EsphomeError(
                    f"{name} must be a hex literal (e.g. 0x8000), got (no value)"
                )
        for name, body in defines.items():
            if not name.startswith("MMU_") or "=" not in body:
                # Valueless flags (MMU_IRAM_HEAP) are legitimate switches
                continue
            # Every valued MMU_* reaches the linker-script preprocessor; a
            # bare or non-numeric value would corrupt it and fail far away
            # in ld. The two segment sizes must additionally be hex:
            # build_surgery's segment parser cannot read decimal back.
            value = body.partition("=")[2]
            rule = (
                _MMU_HEX_VALUE_RE if name in _MMU_SEGMENT_SIZE_NAMES else _MMU_VALUE_RE
            )
            if not rule.fullmatch(value):
                shape = (
                    "a hex literal (e.g. 0x8000)"
                    if name in _MMU_SEGMENT_SIZE_NAMES
                    else "a numeric literal"
                )
                raise EsphomeError(
                    f"{name} must be {shape}, got {value or '(no value)'}"
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
        lwip_lib=lwip.lib,
        exceptions="PIO_FRAMEWORK_ARDUINO_ENABLE_EXCEPTIONS" in defines,
        vtables=vtables,
        fp_in_irom="FP_IN_IROM" in defines,
        knob_defines=knob_defines,
        mmu_defines=mmu,
    )


_INCOMPLETE_INSTALL = "Arduino toolchain install is incomplete"


def _filter_link_flags(unflags: set[str]) -> list[str]:
    """_LINKFLAGS minus ``unflags``, pair-aware: unflagging a symbol also
    drops the ``-u`` that carried it, so no dangling operand-less flag
    reaches ld as the next token's consumer."""
    out: list[str] = []
    it = iter(_LINKFLAGS)
    for tok in it:
        if tok == "-u":
            symbol = next(it)
            if symbol not in unflags:
                out += [tok, symbol]
        elif tok not in unflags:
            out.append(tok)
    return out


def _active_flash_ld_name(flash_ld_name: str) -> str:
    """The flash linker-script filename the link uses (testing mode renames
    the surgically patched copy)."""
    return (
        f"{_TESTING_LD_PREFIX}{flash_ld_name}" if CORE.testing_mode else flash_ld_name
    )


def _flash_ld_name(board: str) -> str:
    """The flash linker script: the board's, or a routed user override.

    Published configs override board_build.ldscript to reserve a
    filesystem region or correct a board's assumed flash size; a bare
    name is required because the script resolves via the -L search path.
    """
    override = _pio_option("board_build.ldscript", "")
    if not override:
        # The same shared rule the PlatformIO path pins (layout
        # preservation, see boards.board_ld_script)
        return board_ld_script(BOARDS[board])
    if Path(override).name != override:
        raise EsphomeError(
            f"board_build.ldscript must be a bare script name, got {override!r}"
        )
    return override


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
    ``flash_mode`` also lands unquoted: callers pass it pre-validated
    against ``BUILD_FLASH_MODES`` (cv.one_of at config time, the
    ``_FLASH_MODES`` check at the emission half's read site).
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
        # _lexed_build_flags warned about and dropped any bare -I/-D/-L/-l
        if tok.startswith("-Wl,"):
            link_flags.append(_shell_token(tok))
        elif tok.startswith("-L"):
            lib_dirs.append(Path(tok[2:]))
        elif tok.startswith("-l"):
            libs.append(tok[2:])
        else:
            if tok.startswith(_PLAIN_DRIVER_LINK_PREFIXES):
                # Driver options with no -Wl, spelling; ld would reject them
                raise EsphomeError(
                    f"Link flag {tok} in build_flags is not supported by the "
                    "native toolchain"
                )
            if tok in _PLAIN_LINKER_FLAGS or tok.startswith(_PLAIN_LINKER_PREFIXES):
                raise EsphomeError(
                    f"Linker flag {tok} in build_flags is not routed to the "
                    "link line; use the -Wl, form"
                )
            if tok.startswith("-") and not tok.startswith(_COMPILE_FLAG_PREFIXES):
                # The linker deny lists are not exhaustive; an unlisted
                # link-only spelling would be inert on the -c compile line,
                # so at least surface the odd shape
                _LOGGER.warning(
                    "Build flag %s is not a recognized compile-flag shape; "
                    "it is passed to the compile line only",
                    tok,
                )
            compile_flags.append(_shell_token(tok))
    return compile_flags, link_flags, lib_dirs, libs


# Recognized compile-flag shapes: the allow-list feeding the fall-through
# warning in _project_flags (an unlisted link-only spelling still reaches
# the compile line, but not silently)
_COMPILE_FLAG_PREFIXES = (
    "-D",
    "-I",
    "-U",
    "-W",
    "-f",
    "-m",
    "-O",
    "-g",
    "-std=",
    "-include",
)
# Plain-form linker flags rejected by _project_flags: inert on a -c compile
# line, so the firmware would silently lack the requested link behavior.
# Best-effort, not exhaustive; see _COMPILE_FLAG_PREFIXES above.
_PLAIN_LINKER_FLAGS = (
    "-u",
    "-e",
    "-s",
    "-static",
    "-nostartfiles",
    "-nodefaultlibs",
    "-nostdlib",
    "-rdynamic",
)
# The subset whose next token is an operand; unflagging the bare flag
# would strand the operand. Operand-less members of the list above filter
# whole-token from both the compile and link lines, as PlatformIO allows.
_PLAIN_LINKER_OPERAND_FLAGS = ("-u", "-e")
_PLAIN_LINKER_PREFIXES = ("-T", "-Xlinker")
# Driver options, not ld options: -Wl, has no equivalent for these
_PLAIN_DRIVER_LINK_PREFIXES = ("-fuse-ld=", "--specs=", "-specs=")


def _collect_sources(root: Path, exclude: Collection[str] = frozenset()) -> list[Path]:
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


def _write_note(path: Path, text: str, *, warn: bool = False) -> bool:
    """Best-effort bookkeeping write; a failure never fails the build.

    ``warn`` marks notes whose loss drops a diagnostic on later cached
    builds; a lost stamp only costs a cache miss and stays at debug.
    Returns whether the write persisted, so a lost warn note can veto
    the cache stamp and keep the diagnostic re-derivable.
    """
    try:
        path.write_text(text, encoding="utf-8")
    except OSError as err:
        log = _LOGGER.warning if warn else _LOGGER.debug
        log("Could not write %s: %s", path, err)
        return False
    return True


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
    header = _sdk_ld_dir(framework) / _COMMON_LD_HEADER
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
        # shlex.join: a spaced path stays one quoted element, so two
        # different cmd lists can never collide to the same stamp string
        shlex.join(cmd)
        + f" testing={CORE.testing_mode}"
        + f" header={_stat_sig(header)}"
        + f" gcc={_stat_sig(gcc)}"
        + f" {build_surgery.surgery_fingerprint()}"
    )

    stderr_note = ld_dir / f".{_COMMON_LD_NAME}.stderr"

    def _note_digest() -> str:
        # The note is an output like the script itself; folding its state
        # into the stamp makes an externally removed or edited note a cache
        # miss that re-runs -E and re-derives the diagnostic
        if not stderr_note.is_file():
            return "none"
        return hashlib.sha256(stderr_note.read_bytes()).hexdigest()

    def _cached_ld_is_valid() -> bool:
        # Any damaged cache regenerates; never abort the build over it. The
        # stamp records the sha256 of the content written, so an externally
        # edited script regenerates too.
        try:
            if not (output.is_file() and stamp.is_file()):
                return False
            rest, sep, digest = stamp.read_text(encoding="utf-8").rpartition(
                " content="
            )
            inputs, note_sep, note_digest = rest.rpartition(" note=")
            return (
                bool(sep)
                and bool(note_sep)
                and inputs == stamp_content
                and note_digest == _note_digest()
                and hashlib.sha256(output.read_bytes()).hexdigest() == digest
            )
        except (OSError, UnicodeDecodeError):
            return False

    if not _cached_ld_is_valid():
        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                check=False,
                close_fds=False,
            )
        except OSError as err:
            # A half-extracted or half-deleted toolchain cache reaches here
            raise EsphomeError(f"Could not run {gcc}: {err}; {_CLEAN_HINT}") from err
        # Localized gcc diagnostics on a non-UTF-8 console must degrade,
        # not UnicodeDecodeError the build; the script itself (below) is
        # decoded strictly instead, so a mangled byte can never be cached
        stderr_text = result.stderr.decode("utf-8", errors="replace")
        if result.returncode != 0:
            raise EsphomeError(f"Generating the linker script failed:\n{stderr_text}")
        note_persisted = True
        if stderr_text.strip():
            # Preprocessor warnings on the success path must reach the user
            # on this and every later cached build (see the re-emit below)
            _LOGGER.warning("Linker-script preprocessor: %s", stderr_text.strip())
            note_persisted = _write_note(stderr_note, stderr_text.strip(), warn=True)
        else:
            try:
                stderr_note.unlink(missing_ok=True)
            except OSError as err:
                # A kept stale note would re-emit an obsolete diagnostic on
                # every cache hit; skip the stamp so -E re-derives the truth
                _LOGGER.warning(
                    "Could not remove %s (%s); the linker script will "
                    "regenerate every build until it is removable; %s",
                    stderr_note,
                    err,
                    _CLEAN_HINT,
                )
                note_persisted = False
        try:
            stdout_text = result.stdout.decode("utf-8")
        except UnicodeDecodeError as err:
            # -CC keeps header comments verbatim; a non-UTF-8 byte replaced
            # with U+FFFD would be cached as valid for the build dir's life
            raise EsphomeError(
                f"Preprocessed linker script from {header} is not UTF-8: "
                f"{err}; {_CLEAN_HINT}"
            ) from err
        if "SECTIONS" not in stdout_text:
            # A degenerate zero-exit run must not be stamped as a good cache
            raise EsphomeError(
                f"Generated linker script is missing its SECTIONS block; {_CLEAN_HINT}"
            )
        content = _apply_surgery(build_surgery.relocate_ratetable, stdout_text)
        if CORE.testing_mode:
            content = _apply_surgery(
                build_surgery.apply_testing_memory_patches, content, ("iram1_0_seg",)
            )
        write_file_if_changed(output, content)
        if note_persisted:
            # An unstamped cache re-runs -E next build, re-deriving the
            # diagnostic the lost note would have re-emitted
            _write_note(
                stamp,
                f"{stamp_content} note={_note_digest()} "
                f"content={hashlib.sha256(content.encode('utf-8')).hexdigest()}",
            )
    elif stderr_note.is_file():
        # Re-emit cached preprocessor warnings on cache hits
        try:
            _LOGGER.warning(
                "Linker-script preprocessor: %s",
                stderr_note.read_text(encoding="utf-8"),
            )
        except (OSError, UnicodeDecodeError) as err:
            _LOGGER.warning(
                "A cached linker-script preprocessor diagnostic exists at %s "
                "but could not be read: %s",
                stderr_note,
                err,
            )

    if CORE.testing_mode:
        _generate_testing_flash_ld(framework, ld_dir, flash_ld_name)


def _generate_testing_flash_ld(
    framework: Path, ld_dir: Path, flash_ld_name: str
) -> None:
    """A patched copy of the flash ld in the build dir; resolved through the
    same -L path as the SDK original it shadows."""
    flash_ld = _sdk_ld_dir(framework) / flash_ld_name
    try:
        flash_ld_text = flash_ld.read_text(encoding="utf-8")
    except OSError as err:
        # Same half-extracted-cache hazard as the preprocessor spawn
        raise EsphomeError(f"Could not read {flash_ld}: {err}; {_CLEAN_HINT}") from err
    patched_flash_ld = _apply_surgery(
        build_surgery.apply_testing_memory_patches,
        flash_ld_text,
        ("dram0_0_seg", "irom0_0_seg"),
    )
    write_file_if_changed(
        ld_dir / f"{_TESTING_LD_PREFIX}{flash_ld_name}", patched_flash_ld
    )


def _ninja_compile_edges(
    lines: list[str],
    sources: list[Path],
    root: Path,
    group: str,
    flags: str = "",
    cxx_override: tuple[str, str] | None = None,
) -> list[str]:
    """Emit compile edges for ``sources``; return the object paths.

    ``cxx_override`` is a (flags, implicit-dep) pair applied to C++ edges
    only, replacing ``flags`` (used for the precompiled header).
    """
    objects = []
    for src in sources:
        rel = src.relative_to(root).as_posix()
        obj = f"obj/{group}/{rel}.o"
        escaped_obj = _e(obj)
        kind = SOURCE_KIND_FOR_SUFFIX[src.suffix]
        override = cxx_override if kind == "cxx" and cxx_override else None
        implicit = f" | {override[1]}" if override else ""
        lines.append(f"build {escaped_obj}: {kind} {_e(src)}{implicit}")
        edge_flags = override[0] if override else flags
        if edge_flags:
            lines.append(f"  flags = {edge_flags}")
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
    # Config validation already gates boards;
    # kept as defense-in-depth for direct calls, since CONF_BOARD itself is
    # a free-form string
    if board not in ESP8266_BOARD_BUILD:
        raise EsphomeError(f"Board '{board}' is not supported by the native toolchain")
    board_build = ESP8266_BOARD_BUILD[board]
    # From the same producer the PlatformIO path reads (one source)
    flash_mode = _pio_option("board_build.flash_mode", "dout")
    if flash_mode not in _FLASH_MODES:
        # Lands unquoted in the elf2bin command and a -D body; validation
        # (cv.one_of on board_flash_mode) already gates it, defense-in-depth
        raise EsphomeError(f"Invalid flash mode {flash_mode!r}")
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
    defines = _defines_flags(config, flash_mode, board, board_build["defines"])
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
    # _LINKFLAGS stores -u and its operand as two tokens; unflagging the
    # bare -u would strip all seven and leave the operands as ld "input
    # files" with an error pointing nowhere near build_unflags
    if plain := sorted(
        u
        for u in unflags
        if u in _PLAIN_LINKER_OPERAND_FLAGS or u.startswith(_PLAIN_LINKER_PREFIXES)
    ):
        raise EsphomeError(
            f"build_unflags cannot remove plain linker flag(s) "
            f"{', '.join(plain)}; unflag the full -Wl, form or the symbol"
        )
    cflags, cxxflags, asflags = (
        [f for f in flags if f not in unflags] for flags in (cflags, cxxflags, asflags)
    )
    link_flags = _filter_link_flags(unflags)
    if esp8266_data[KEY_SCANF_FLOAT]:
        link_flags += ["-u", "_scanf_float"]
    link_flags += project_link_flags
    link_flags += [_shell_token(flag) for lib in libraries for flag in lib.link_flags]
    flash_ld = _active_flash_ld_name(flash_ld_name)
    # A user-overridden script name re-quotes like every other user token
    link_flags += ["-T", _shell_token(flash_ld)]

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
        # Rule names match SOURCE_KIND_FOR_SUFFIX values (c, cxx, asm, aspp)
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
        "rule aspp",
        "  command = $ccache $cc -MMD -MF $out.d -x assembler-with-cpp $asflags $flags -c $in -o $out",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = AS $out",
        # No $ccache: the .gch is compiled once per build dir and ccache
        # cannot cache it usefully (its bytes embed build-dir paths)
        "rule pch",
        "  command = $cxx -MMD -MF $out.d -x c++-header $cxxflags $flags -c $in -o $out",
        "  depfile = $out.d",
        "  deps = gcc",
        "  description = PCH $out",
        # Plain assembler, as SCons's ASCOM: no preprocessor, so no
        # depfile and no $flags (defines/includes) either
        "rule asm",
        "  command = $ccache $cc -x assembler $asflags -c $in -o $out",
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
        # --flash_size deliberately stays board-derived, as under
        # PlatformIO (which reads upload.maximum_size, not the ldscript).
        f"  command = $python {_q(framework / 'tools' / 'elf2bin.py')} --eboot {_q(framework / 'bootloaders' / 'eboot' / 'eboot.elf')} --app $in --flash_mode {flash_mode} --flash_freq {_FLASH_FREQ_MHZ} --flash_size {_flash_size_str(BOARDS[board][KEY_FLASH_SIZE])} --path {_q(toolchain_bin)} --out $out",
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

    # One source of truth with the PlatformIO path: esp8266/__init__ pins
    # build_src_flags (the throw_stubs force-include); -include paths
    # resolve against the source root
    src_other: list[str] = []
    src_includes: list[str] = []
    src_it = iter(
        lex_build_flags(_pio_option("build_src_flags", ""), "build_src_flags")
    )
    for tok in src_it:
        if tok == "-include":
            header = next(src_it, "")
            if not header:
                raise EsphomeError(
                    "build_src_flags has a trailing '-include' with no header"
                )
            src_includes.append(header)
        else:
            src_other.append(_shell_token(tok))
    include_flags = [f"-include {_q(src_dir / h)}" for h in src_includes]
    # One shared variable instead of repeating the flags line on every src
    # edge (hundreds of edges in a real project)
    lines.append(f"srcflags = {' '.join(src_other + include_flags)}")
    src_cxx_override = None
    if pch_enabled() and "-include" in cxxflags:
        # GCC only loads a .gch while no tokens precede it, and the cxx rule
        # expands $cxxflags before $flags: a user -include in build_flags
        # means every TU would silently skip the .gch
        _LOGGER.warning(
            "A -include in build_flags prevents the precompiled header from "
            "loading; compiling without it"
        )
    elif pch_enabled():
        # C++ src edges swap the force-includes for one precompiled prefix
        # header holding the same content plus defines.h; C and assembly
        # edges keep srcflags (a .gch is a C++ artifact)
        # The opt-out hint matters when a toolchain rejects its own .gch:
        # the build stays correct but every TU warns via -Winvalid-pch
        pch_header = build_dir / PCH_HEADER_NAME
        pch_includes = (*src_includes, PCH_CORE_HEADER)
        pch_text = pch_header_text(pch_includes)
        checksum = None
        try:
            if ccache:
                # The .sum sidecar only exists for CCACHE_PCH_EXTSUM; ninja's
                # depfile handles staleness. Mirror CCACHE_BASEDIR: strip the
                # per-device build path so identically-configured devices
                # produce identical .sum files and share cache entries
                # Raw path too: a symlinked build dir resolves differently
                flags_id = (
                    " ".join(cxxflags)
                    .replace(effective_ccache_basedir(), "")
                    .replace(str(CORE.build_path), "")
                )
                # The header text covers include order, which the sorted
                # closure alone does not
                checksum = pch_checksum(
                    src_dir,
                    pch_includes,
                    (
                        pch_text,
                        str(paths.framework),
                        str(paths.toolchain),
                        flags_id,
                    ),
                )
        except (OSError, UnicodeError) as err:
            # Identity unknown: a stale cache entry must never be served
            _LOGGER.warning(
                "Could not establish the pch identity; compiling without it: %s", err
            )
        else:
            _LOGGER.info(
                "Compiling with a precompiled header "
                "(set ESPHOME_PCH_ENABLE=0 to disable)"
            )
            write_file_if_changed(pch_header, pch_text)
            if checksum is not None:
                write_file_if_changed(
                    build_dir / f"{PCH_HEADER_NAME}.gch.sum", checksum + "\n"
                )
            gch = _e(f"{PCH_HEADER_NAME}.gch")
            lines.append(f"build {gch}: pch {_e(pch_header)}")
            if src_other:
                lines.append(f"  flags = {' '.join(src_other)}")
            # Relative -include (resolved from the ninja cwd, where the header
            # lives): an absolute path would put the per-device build path on
            # every compile command and defeat cross-device ccache sharing
            cxx_parts = src_other + [f"-Winvalid-pch -include {PCH_HEADER_NAME}"]
            lines.append(f"srccxxflags = {' '.join(cxx_parts)}")
            src_cxx_override = ("$srccxxflags", gch)
    src_objs = _ninja_compile_edges(
        lines,
        _collect_sources(src_dir),
        src_dir,
        "src",
        flags="$srcflags",
        cxx_override=src_cxx_override,
    )

    ld_deps = [f"ld/{_COMMON_LD_NAME}"]
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
    re-resolving the framework version. A user-shipped override living in a
    custom -L dir resolves to a nonexistent path here; the size consumer
    warns and skips the Flash summary then.
    """
    name = _active_flash_ld_name(_flash_ld_name(CORE.data[KEY_ESP8266][KEY_BOARD]))
    if CORE.testing_mode:
        return build_dir / "ld" / name
    return paths.framework / "tools" / "sdk" / "ld" / name


def _flash_size_str(flash_size: int) -> str:
    """Flash size argument for elf2bin (e.g. ``4M``, ``512K``)."""
    mb = 1024 * 1024
    return f"{flash_size // mb}M" if flash_size >= mb else f"{flash_size // 1024}K"
