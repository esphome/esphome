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

from dataclasses import dataclass, field
import logging
from pathlib import Path
import re
import subprocess

from esphome.components.esp8266 import build_surgery
from esphome.core import CORE, EsphomeError
from esphome.helpers import mkdir_p, write_file_if_changed
from esphome.platformio.library import join_flag_args, split_flag_entry

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
        ["MMU_IRAM_SIZE=0xC000", "MMU_ICACHE_SIZE=0x4000"],
    ),
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
)
_MMU_DEFAULT = ("MMU_IRAM_SIZE=0x8000", "MMU_ICACHE_SIZE=0x8000")

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


def _flag_defines() -> dict[str, str]:
    """Map define name -> full ``NAME[=VALUE]`` for every -D build flag."""
    defines: dict[str, str] = {}
    for flag in CORE.build_flags:
        # Shell-lex multi-token entries the way PlatformIO does, so a knob
        # in "-DKNOB -DOTHER" or a spaced "-D KNOB" is still detected;
        # single tokens pass verbatim to keep quoting in their bodies intact.
        tokens = (
            join_flag_args(split_flag_entry(flag, "esphome"), "esphome")
            if " " in flag
            else (flag,)
        )
        for tok in tokens:
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
    vtables = next(
        (name for name in sorted(defines) if name.startswith("VTABLES_IN_")),
        "VTABLES_IN_FLASH",
    )

    mmu = next((variant for knob, variant in _MMU_VARIANTS if knob in defines), None)
    if mmu is None:
        if "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM" in defines:
            if "MMU_IRAM_SIZE" not in defines or "MMU_ICACHE_SIZE" not in defines:
                raise EsphomeError(
                    "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM requires MMU_IRAM_SIZE and "
                    "MMU_ICACHE_SIZE build flags"
                )
            # Sorted so build.ninja and the linker-script stamp stay
            # byte-stable across runs (the flag set has no deterministic
            # iteration order).
            mmu = sorted(
                body for name, body in defines.items() if name.startswith("MMU_")
            )
        else:
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


def _quote_arg(tok: str) -> str:
    """Wrap a token in double quotes with the Windows argv rule.

    Same escaping rule as ``subprocess.list2cmdline``: a backslash run
    doubles only immediately before a quote (or the closing quote), and the
    quote itself is escaped. POSIX sh parses the result identically for
    backslashes and quotes. ``$`` must already be doubled for ninja.
    """
    quoted = re.sub(r'(\\*)"', lambda m: m.group(1) * 2 + '\\"', tok)
    quoted = re.sub(r"(\\+)\Z", lambda m: m.group(1) * 2, quoted)
    return f'"{quoted}"'


_NEEDS_QUOTE = re.compile(r'[\s"\']')


def _shell_token(tok: str) -> str:
    """Quote a lexed token only when needed; ``_q`` force-quotes paths.

    Lexing strips the quoting a user wrote (``-DX="a b"`` becomes the single
    token ``-DX=a b``); re-quote on the way out so the compiler receives the
    same argv element SCons would pass under PlatformIO. After ninja
    un-doubles ``$$``, sh still expands ``$VAR`` while CreateProcess passes
    it literally -- the same divergence SCons-under-sh has, so this stays
    PlatformIO parity.
    """
    tok = tok.replace("$", "$$")  # ninja would expand a bare $ to nothing
    if not _NEEDS_QUOTE.search(tok):
        return tok
    return _quote_arg(tok)


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
    return {
        tok
        for entry in CORE.build_unflags
        for tok in split_flag_entry(entry, "esphome build_unflags")
    }


def _project_flags(
    unflags: set[str] | None = None,
) -> tuple[list[str], list[str], list[Path], list[str]]:
    """Split the ESPHome build flags into compile, linker, -L, and -l lists.

    Every entry is shell-lexed the way PlatformIO's ``ParseFlags`` does, so a
    linker flag anywhere in an entry reaches the link line and
    ``build_unflags`` matches individual tokens (``-Os`` inside ``-Os -g3``).
    Lexed tokens are re-quoted at emission via ``_shell_token``.
    """
    if unflags is None:
        unflags = _unflag_tokens()
    compile_flags: list[str] = []
    link_flags: list[str] = []
    lib_dirs: list[Path] = []
    libs: list[str] = []
    for flag in sorted(CORE.build_flags):
        for tok in join_flag_args(split_flag_entry(flag, "esphome"), "esphome"):
            if tok in unflags:
                continue
            if tok.startswith("-Wl,"):
                link_flags.append(_shell_token(tok))
            elif tok.startswith("-L"):
                lib_dirs.append(Path(tok[2:]))
            elif tok.startswith("-l"):
                libs.append(tok[2:])
            else:
                compile_flags.append(_shell_token(tok))
    return compile_flags, link_flags, lib_dirs, libs


def generate_ld_scripts(
    paths: dict[str, Path], config: _BuildConfig, flash_ld_name: str
) -> None:
    """Generate the common linker script (and testing-mode flash ld copy).

    Runs the same preprocessor invocation as the PlatformIO builder over
    ``eagle.app.v6.common.ld.h``, then applies ESPHome's surgeries: the wifi
    rate-table DRAM relocation, and enlarged memory segments in testing mode.
    """
    framework = paths["framework_path"]
    gcc = paths["toolchain_path"] / "bin" / "xtensa-lx106-elf-gcc"
    ld_dir = CORE.relative_pioenvs_path(CORE.name, "ld")
    mkdir_p(ld_dir)

    cmd = [str(gcc), "-CC", "-E", "-P", f"-D{config.vtables}"]
    cmd += [f"-D{d}" for d in config.mmu_defines]
    if config.fp_in_irom:
        cmd.append("-DFP_IN_IROM")
    cmd += [
        str(framework / "tools" / "sdk" / "ld" / "eagle.app.v6.common.ld.h"),
        "-o",
        "-",
    ]

    # The inputs are the command line (defines + framework version, which is
    # baked into the paths) plus testing mode; skip the preprocessor spawn on
    # incremental builds when nothing changed.
    output = ld_dir / "local.eagle.app.v6.common.ld"
    stamp = ld_dir / ".local.eagle.app.v6.common.ld.stamp"
    # The surgery constants are inputs too: an edit to build_surgery.py must
    # invalidate existing build dirs, not wait for an esphome clean.
    stamp_content = (
        " ".join(cmd)
        + f" testing={CORE.testing_mode}"
        # One fingerprint instead of enumerating surgery internals here, so
        # any behavioral edit in build_surgery self-invalidates the cache
        + f" {build_surgery.surgery_fingerprint()}"
    )
    if not (
        output.is_file()
        and stamp.is_file()
        and stamp.read_text(encoding="utf-8") == stamp_content
    ):
        result = subprocess.run(
            cmd, capture_output=True, text=True, check=False, close_fds=False
        )
        if result.returncode != 0:
            raise EsphomeError(f"Generating the linker script failed:\n{result.stderr}")
        content = build_surgery.relocate_ratetable(result.stdout)
        if CORE.testing_mode:
            content = build_surgery.apply_testing_memory_patches(
                content, ("iram1_0_seg",)
            )
        write_file_if_changed(output, content)
        stamp.write_text(stamp_content, encoding="utf-8")

    if CORE.testing_mode:
        # A patched copy of the flash ld in the build dir; resolved through
        # the same -L path as the SDK original it shadows.
        flash_ld = framework / "tools" / "sdk" / "ld" / flash_ld_name
        write_file_if_changed(
            ld_dir / f"testing_{flash_ld_name}",
            build_surgery.apply_testing_memory_patches(
                flash_ld.read_text(encoding="utf-8"),
                ("dram0_0_seg", "irom0_0_seg"),
            ),
        )
