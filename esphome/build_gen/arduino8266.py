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

from dataclasses import dataclass, field
import logging
import os
from pathlib import Path
import subprocess
import sys
from typing import TYPE_CHECKING

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
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import get_project_cxx_compile_flags
from esphome.helpers import mkdir_p, write_file_if_changed
from esphome.platformio.library import (
    SOURCE_KIND_FOR_SUFFIX,
    join_flag_args,
    split_flag_entry,
)

if TYPE_CHECKING:
    from esphome.arduino8266.framework import InstalledPaths

_LOGGER = logging.getLogger(__name__)

# Compile rule per source suffix, derived from the shared suffix -> kind map
# so every source extension a library manifest can select has a rule.
_RULE_FOR_KIND = {"c": "cc", "cxx": "cxx", "asm": "asm"}
_RULE_FOR_SUFFIX = {
    suffix: _RULE_FOR_KIND[kind] for suffix, kind in SOURCE_KIND_FOR_SUFFIX.items()
}

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


def _flag_defines(unflags: set[str]) -> dict[str, str]:
    """Map define name -> full ``NAME[=VALUE]`` for every -D build flag."""
    defines: dict[str, str] = {}
    # Sorted so duplicate defines resolve the same way every run: the
    # winner feeds the linker-script preprocessor line, which is also the
    # cache stamp
    for flag in sorted(CORE.build_flags):
        # Shell-lex every entry the way PlatformIO's ParseFlags does, so a
        # knob in "-DKNOB -DOTHER", a spaced "-D KNOB", and quoted bodies all
        # read identically to _project_flags (and the compile line).
        tokens = join_flag_args(split_flag_entry(flag, "esphome"), "esphome")
        for tok in tokens:
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
    if unknown := [k for k in vtables_knobs if k not in known_vtables]:
        _LOGGER.warning("Unknown VTABLES_IN_* define(s): %s", ", ".join(unknown))
    if len(vtables_knobs) > 1:
        _LOGGER.warning("Multiple VTABLES_IN_* defines; using %s", vtables_knobs[0])
    vtables = vtables_knobs[0] if vtables_knobs else "VTABLES_IN_FLASH"

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
            if "MMU_IRAM_SIZE" in defines or "MMU_ICACHE_SIZE" in defines:
                # Same diagnostic the PlatformIO builder prints: without the
                # knob the linker script keeps the default layout while the
                # compile line carries the custom sizes
                _LOGGER.warning(
                    "Detected custom MMU flags; use "
                    "-DPIO_FRAMEWORK_ARDUINO_MMU_CUSTOM to disable the "
                    "default configuration"
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
    return ESP8266_LD_SCRIPTS[BOARDS[board][KEY_FLASH_SIZE]][1]


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
    # Joined like _project_flags reads build_flags, so "-D FOO" removes
    # -DFOO in both spellings (PlatformIO's ProcessUnFlags parses the same
    # way) and no bare half can collaterally drop an unrelated token
    return {
        tok
        for entry in CORE.build_unflags
        for tok in join_flag_args(
            split_flag_entry(entry, "esphome build_unflags"),
            "esphome build_unflags",
        )
    }


def _project_flags(
    unflags: set[str],
) -> tuple[list[str], list[str], list[Path], list[str]]:
    """Split the ESPHome build flags into compile, linker, -L, and -l lists.

    Every entry is shell-lexed the way PlatformIO's ``ParseFlags`` does, so a
    linker flag anywhere in an entry reaches the link line and
    ``build_unflags`` matches individual tokens (``-Os`` inside ``-Os -g3``).
    Lexed tokens are re-quoted at emission via ``_shell_token``.
    """
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
                compile_flags.append(_shell_token(tok))
    return compile_flags, link_flags, lib_dirs, libs


def _collect_sources(root: Path, exclude: set[str] = frozenset()) -> list[Path]:
    return sorted(
        p
        for p in root.rglob("*")
        if p.suffix in _RULE_FOR_SUFFIX and p.name not in exclude
    )


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
            _LOGGER.warning("Linker-script preprocessor: %s", result.stderr.strip())
        if "SECTIONS" not in result.stdout:
            # A degenerate zero-exit run must not be stamped as a good cache
            raise EsphomeError(
                "Generated linker script is missing its SECTIONS block; "
                "run 'esphome clean-all' and retry"
            )
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
        lines.append(f"build {escaped_obj}: {_RULE_FOR_SUFFIX[src.suffix]} {_e(src)}")
        if flags:
            lines.append(f"  flags = {flags}")
        # Escaped once here: the returned paths only ever appear in build
        # statements (archive/link inputs), which use ninja escaping.
        objects.append(escaped_obj)
    return objects


def _common_parent(paths: list[Path]) -> Path:
    return Path(os.path.commonpath([str(p.parent) for p in paths]))


def write_project(paths: InstalledPaths) -> bool:
    """Write the ninja build for the current configuration.

    Returns True when ``build.ninja`` changed, so the caller can skip work
    derived purely from it (the compile database) on unchanged builds.
    """
    from esphome.arduino.library import resolve_libraries
    from esphome.arduino8266.framework import ccache_path

    framework = paths.framework
    toolchain_bin = paths.toolchain / "bin"
    build_dir = CORE.relative_pioenvs_path(CORE.name)
    mkdir_p(build_dir)

    unflags = _unflag_tokens()
    flag_defines = _flag_defines(unflags)
    config = _resolve_build_config(flag_defines)
    esp8266_data = CORE.data[KEY_ESP8266]
    board = esp8266_data[KEY_BOARD]
    # Config-time validation rejects unsupported boards before this runs;
    # guard anyway so a bypassing caller fails by name, not KeyError
    if board not in BOARDS or board not in ESP8266_BOARD_BUILD:
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
    for lib in libraries:
        include_dirs += lib.include_dirs

    (
        project_compile_flags,
        project_link_flags,
        project_lib_dirs,
        project_libs,
    ) = _project_flags(unflags)
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
        + get_project_cxx_compile_flags()
    )
    # PlatformIO's ASPPCOM carries defines and includes but not CCFLAGS,
    # so only -D/-I user flags reach assembly there; match it. The tokens
    # are already shell-quoted (per-platform style), so test past a
    # leading quote too.
    asflags = (
        _ASFLAGS
        + defines
        + includes
        + [f for f in project_compile_flags if f.lstrip("\"'").startswith(("-D", "-I"))]
    )

    # build_unflags applies to the framework flag sets too (compile and link),
    # as under PlatformIO (a silently ignored ``build_unflags: -Os`` would
    # diverge between the toolchains).
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
    ccache = ccache_path()

    # $in/$out stay unquoted in the rule commands: ninja shell-escapes its
    # built-in path variables itself when expanding a command (POSIX and
    # Windows), so adding quotes would wrap ninja's own quoting and break
    # space-containing paths. Only literal paths need _q().
    lines = [
        "# Auto-generated by ESPHome",
        "ninja_required_version = 1.5",
        f"cc = {_q(toolchain_bin / 'xtensa-lx106-elf-gcc')}",
        f"cxx = {_q(toolchain_bin / 'xtensa-lx106-elf-g++')}",
        f"python = {_q(sys.executable)}",
        f"buildtool = {_q(build_tool)}",
        f"ccache = {_q(ccache) if ccache else ''}",
        "",
        "rule cc",
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
        f"  command = $python $buildtool ar {_q(toolchain_bin / 'xtensa-lx106-elf-ar')} $out $out.rsp",
        "  rspfile = $out.rsp",
        "  rspfile_content = $in_newline",
        "  description = AR $out",
        "rule link",
        "  command = $cxx -o $out $linkflags @$out.rsp $libdirflags -Wl,--start-group $archives $libflags -Wl,--end-group",
        "  rspfile = $out.rsp",
        "  rspfile_content = $in_newline",
        "  description = LINK $out",
        "rule elf2bin",
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


def get_flash_ld_path(build_dir: Path) -> Path:
    """The flash linker script the link actually uses (for size reporting)."""
    from esphome.arduino8266.framework import (
        framework_package_version,
        get_framework_path,
    )

    name = _active_flash_ld_name(_flash_ld_name(CORE.data[KEY_ESP8266][KEY_BOARD]))
    if CORE.testing_mode:
        return build_dir / "ld" / name
    version = framework_package_version(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    return get_framework_path(version) / "tools" / "sdk" / "ld" / name


def _flash_size_str(flash_size: int) -> str:
    """Flash size argument for elf2bin (e.g. ``4M``, ``512K``)."""
    mb = 1024 * 1024
    return f"{flash_size // mb}M" if flash_size >= mb else f"{flash_size // 1024}K"
