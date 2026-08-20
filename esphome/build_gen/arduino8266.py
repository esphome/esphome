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
import re
import subprocess
import sys

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
from esphome.platformio.library import join_flag_args, split_flag_entry

_LOGGER = logging.getLogger(__name__)

# Compile rule per source suffix; keys must cover SRC_FILE_EXTENSIONS so any
# source a library manifest selects has a rule (pinned by a drift test).
_RULE_FOR_SUFFIX = {
    ".c": "cc",
    ".cpp": "cxx",
    ".cc": "cxx",
    ".cxx": "cxx",
    ".c++": "cxx",
    ".S": "asm",
    ".spp": "asm",
    ".SPP": "asm",
    ".sx": "asm",
    ".s": "asm",
    ".asm": "asm",
    ".ASM": "asm",
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
        # in "-DKNOB -DOTHER" is still detected; single tokens pass verbatim
        # to keep any quoting in their bodies intact.
        for tok in split_flag_entry(flag, "esphome") if " " in flag else (flag,):
            if tok.startswith("-D"):
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

    if "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48" in defines:
        mmu = ["MMU_IRAM_SIZE=0xC000", "MMU_ICACHE_SIZE=0x4000"]
    elif "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM48_SECHEAP_SHARED" in defines:
        mmu = ["MMU_IRAM_SIZE=0xC000", "MMU_ICACHE_SIZE=0x4000", "MMU_IRAM_HEAP"]
    elif "PIO_FRAMEWORK_ARDUINO_MMU_CACHE16_IRAM32_SECHEAP_NOTSHARED" in defines:
        mmu = [
            "MMU_IRAM_SIZE=0x8000",
            "MMU_ICACHE_SIZE=0x4000",
            "MMU_SEC_HEAP_SIZE=0x4000",
            "MMU_SEC_HEAP=0x40108000",
        ]
    elif "PIO_FRAMEWORK_ARDUINO_MMU_EXTERNAL_128K" in defines:
        mmu = [
            "MMU_IRAM_SIZE=0x8000",
            "MMU_ICACHE_SIZE=0x8000",
            "MMU_EXTERNAL_HEAP=128",
        ]
    elif "PIO_FRAMEWORK_ARDUINO_MMU_EXTERNAL_1024K" in defines:
        mmu = [
            "MMU_IRAM_SIZE=0x8000",
            "MMU_ICACHE_SIZE=0x8000",
            "MMU_EXTERNAL_HEAP=256",
        ]
    elif "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM" in defines:
        if "MMU_IRAM_SIZE" not in defines or "MMU_ICACHE_SIZE" not in defines:
            raise EsphomeError(
                "PIO_FRAMEWORK_ARDUINO_MMU_CUSTOM requires MMU_IRAM_SIZE and "
                "MMU_ICACHE_SIZE build flags"
            )
        # Sorted so build.ninja and the linker-script stamp stay byte-stable
        # across runs (the flag set has no deterministic iteration order).
        mmu = sorted(body for name, body in defines.items() if name.startswith("MMU_"))
    else:
        mmu = ["MMU_IRAM_SIZE=0x8000", "MMU_ICACHE_SIZE=0x8000"]

    return _BuildConfig(
        nonosdk=nonosdk,
        lwip_lib=lwip_lib,
        exceptions="PIO_FRAMEWORK_ARDUINO_ENABLE_EXCEPTIONS" in defines,
        vtables=vtables,
        fp_in_irom="FP_IN_IROM" in defines,
        knob_defines=knob_defines,
        mmu_defines=mmu,
    )


def _flash_ld_name(board: str) -> str:
    return ESP8266_LD_SCRIPTS[BOARDS[board][KEY_FLASH_SIZE]][1]


def _e(value) -> str:
    """Escape a path or token for a ninja file."""
    return str(value).replace("$", "$$").replace(":", "$:").replace(" ", "$ ")


def _q(value) -> str:
    """Quote a path for use inside a ninja command line (shell/CreateProcess).

    ``$`` doubles so ninja passes it through literally instead of expanding
    an (empty) ninja variable.
    """
    return '"' + str(value).replace("$", "$$") + '"'


_NEEDS_QUOTE = re.compile(r'[\s"\']')


def _shell_token(tok: str) -> str:
    """Quote a lexed token for the ninja command line; ``_q`` is for paths.

    Lexing strips the quoting a user wrote (``-DX="a b"`` becomes the single
    token ``-DX=a b``); re-quote on the way out so the compiler receives the
    same argv element SCons would pass under PlatformIO. Uses the Windows
    argv quoting rule, which POSIX sh parses identically inside double
    quotes: a backslash run doubles only immediately before a quote.
    """
    tok = tok.replace("$", "$$")  # ninja would expand a bare $ to nothing
    if not _NEEDS_QUOTE.search(tok):
        return tok
    quoted = re.sub(r'(\\*)"', lambda m: m.group(1) * 2 + '\\"', tok)
    quoted = re.sub(r"(\\+)\Z", lambda m: m.group(1) * 2, quoted)
    return f'"{quoted}"'


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
                lib_dirs.append(Path(tok[2:]))
            elif tok.startswith("-l"):
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
        + f" {build_surgery.RATETABLE_RULE}"
        + f" {build_surgery.TESTING_IRAM_SIZE}"
        + f" {build_surgery.TESTING_DRAM_SIZE}"
        + f" {build_surgery.TESTING_FLASH_SIZE}"
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
                content, require=("iram1_0_seg",)
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
                require=("dram0_0_seg", "irom0_0_seg"),
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
        lines.append(f"build {_e(obj)}: {_RULE_FOR_SUFFIX[src.suffix]} {_e(src)}")
        if flags:
            lines.append(f"  flags = {flags}")
        # Escaped once here: the returned paths only ever appear in build
        # statements (archive/link inputs), which use ninja escaping.
        objects.append(_e(obj))
    return objects


def _common_parent(paths: list[Path]) -> Path:
    return Path(os.path.commonpath([str(p.parent) for p in paths]))


def write_project(paths: dict[str, Path]) -> bool:
    """Write the ninja build for the current configuration.

    Returns True when ``build.ninja`` changed, so the caller can skip work
    derived purely from it (the compile database) on unchanged builds.
    """
    from esphome.arduino8266.component import resolve_libraries
    from esphome.arduino8266.framework import ccache_path

    framework = paths["framework_path"]
    toolchain_bin = paths["toolchain_path"] / "bin"
    build_dir = CORE.relative_pioenvs_path(CORE.name)
    mkdir_p(build_dir)

    flag_defines = _flag_defines()
    config = _resolve_build_config(flag_defines)
    esp8266_data = CORE.data[KEY_ESP8266]
    # Board support was validated at config time (_validate_native_toolchain).
    board = esp8266_data[KEY_BOARD]
    board_build = ESP8266_BOARD_BUILD[board]
    flash_ld_name = _flash_ld_name(board)

    generate_ld_scripts(paths, config, flash_ld_name)

    sdk = framework / "tools" / "sdk"
    core_dir = framework / "cores" / "esp8266"
    variant_dir = framework / "variants" / board_build["variant"]
    src_dir = CORE.relative_src_path()

    libraries = resolve_libraries(framework)

    # A missing install directory would otherwise surface as a wall of
    # include errors; failing here names the path instead.
    include_dirs = [
        src_dir,
        sdk / "include",
        core_dir,
        paths["toolchain_path"] / "include",
        sdk / "lwip2" / "include",
        variant_dir,
    ]
    for required in include_dirs:
        if not required.is_dir():
            raise EsphomeError(
                f"Arduino toolchain install is incomplete: missing {required}; "
                "run 'esphome clean-all' and retry"
            )
    for lib in libraries:
        include_dirs += lib.include_dirs

    unflags = _unflag_tokens()
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
    asflags = _ASFLAGS + defines + includes + project_compile_flags

    # build_unflags applies to the framework flag sets too (compile and link),
    # as under PlatformIO (a silently ignored ``build_unflags: -Os`` would
    # diverge between the toolchains).
    cflags = [f for f in cflags if f not in unflags]
    cxxflags = [f for f in cxxflags if f not in unflags]
    asflags = [f for f in asflags if f not in unflags]

    link_flags = [f for f in _LINKFLAGS if f not in unflags]
    if esp8266_data[KEY_SCANF_FLOAT]:
        link_flags += ["-u", "_scanf_float"]
    link_flags += project_link_flags
    link_flags += [flag for lib in libraries for flag in lib.link_flags]
    flash_ld = f"testing_{flash_ld_name}" if CORE.testing_mode else flash_ld_name
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

    build_tool = Path(__file__).parent.parent / "arduino8266" / "build_tool.py"
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
        f"  command = $python {_q(framework / 'tools' / 'elf2bin.py')} --eboot {_q(framework / 'bootloaders' / 'eboot' / 'eboot.elf')} --app $in --flash_mode {esp8266_data[KEY_FLASH_MODE]} --flash_freq 40 --flash_size {_flash_size_str(flash_ld_name)} --path {_q(toolchain_bin)} --out $out",
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
        f"libflags = {' '.join(f'-l{lib}' for lib in system_libs)}",
        "",
    ]

    core_exclude = set(_CORE_EXCLUDE_ALWAYS)
    if "USE_ESP8266_WAVEFORM_STUBS" in flag_defines:
        core_exclude |= _CORE_EXCLUDE_WAVEFORM

    archives = []
    variant_sources = _collect_sources(variant_dir) if variant_dir.is_dir() else []
    if variant_sources:
        objs = _ninja_compile_edges(lines, variant_sources, variant_dir, "variant")
        lines.append(f"build libFrameworkArduinoVariant.a: ar {' '.join(objs)}")
        archives.append("libFrameworkArduinoVariant.a")

    core_objs = _ninja_compile_edges(
        lines, _collect_sources(core_dir, core_exclude), core_dir, "core"
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
        archive = f"lib{lib.name}.a"
        lines.append(f"build {_e(archive)}: ar {' '.join(objs)}")
        archives.append(archive)

    src_extra = f"-include {_q(src_dir / 'esphome' / 'components' / 'esp8266' / 'throw_stubs.h')}"
    src_objs = _ninja_compile_edges(
        lines, _collect_sources(src_dir), src_dir, "src", flags=src_extra
    )

    ld_deps = ["ld/local.eagle.app.v6.common.ld"]
    if CORE.testing_mode:
        ld_deps.append(f"ld/{flash_ld}")
    lines.append(
        f"build firmware.elf: link {' '.join(src_objs)} | "
        f"{' '.join(_e(a) for a in archives)} {' '.join(_e(d) for d in ld_deps)}"
    )
    lines.append(f"  archives = {' '.join(archives)}")
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

    name = _flash_ld_name(CORE.data[KEY_ESP8266][KEY_BOARD])
    if CORE.testing_mode:
        return build_dir / "ld" / f"testing_{name}"
    version = framework_package_version(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION])
    return get_framework_path(version) / "tools" / "sdk" / "ld" / name


def _flash_size_str(flash_ld_name: str) -> str:
    """Flash size for elf2bin, derived from the ld script name (PIO logic)."""
    match = re.search(r"\.flash\.(\d+)([mk])", flash_ld_name)
    if not match:
        raise EsphomeError(f"Cannot parse flash size from {flash_ld_name}")
    return f"{match.group(1)}{match.group(2).upper()}"
