"""Native ninja build generator for the host platform.

Emits ``build.ninja`` under ``.pioenvs/<name>/``: every source in the
generated ``src/`` tree plus the resolved registry libraries compiles with
the machine's compiler and links into ``program``, the name PlatformIO's
native platform produced. Build flags route the way SCons's ``ParseFlags``
did under PlatformIO: ``-D``/``-I``/``-std=``/``-W`` shapes reach the
compile lines only, ``-l``/``-L``/``-Wl,`` the link line only, everything
else both.
"""

from __future__ import annotations

from collections.abc import Iterable
import logging
import os
from pathlib import Path
import subprocess
import sys
from typing import TYPE_CHECKING

from esphome.build_helpers.ninja import escape as _e, quote_path as _q, shell_token
from esphome.build_helpers.ninja_gen import (
    PATH_ARG_FLAGS,
    Flag,
    anchor_path_flag,
    ar_rule_lines,
    collect_sources,
    compile_edges,
    compile_rule_lines,
    library_edges,
    pch_edges,
    pch_rule_lines,
    tool_lines,
)
from esphome.build_helpers.pch import PCH_DEFAULT_HEADERS, pch_enabled
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import get_project_cxx_compile_flags
from esphome.helpers import mkdir_p, write_file_if_changed
from esphome.host.toolchain import PROGRAM_NAME, HostCompilers, find_tool, get_build_dir
from esphome.platformio.library import lex_build_flags

if TYPE_CHECKING:
    from esphome.arduino.library import ArduinoLibrary

_LOGGER = logging.getLogger(__name__)

# The PlatformIO platform the host built under; registry manifests declare
# compatibility against it, as lib_compat_mode=strict checked before
PIO_PLATFORM = "native"
# Namespaces the shared library download cache (pio_components/host/)
LIBRARY_CACHE_KEY = "host"

# Flag shapes that only the compiler understands; dropped from the link line
_COMPILE_ONLY_PREFIXES = ("-D", "-U", "-I", "-std=", "-W", *PATH_ARG_FLAGS)
# Flag shapes that only the linker consumes; inert on a -c compile line
_LINK_ONLY_PREFIXES = ("-l", "-L", "-Wl,")
# Link-only flags whose argument is the next token
_LINK_ONLY_ARG_FLAGS = ("-framework", "-Xlinker", "-z")


def parse_flags(entries: Iterable[str], owner: str) -> list[Flag]:
    """Lex build flag entries into flags, each with its argument.

    Entries are a set, so their order is not the user's: a flag and the
    argument it takes as the next token must share one entry.
    """
    flags: list[Flag] = []
    for entry in entries:
        it = iter(lex_build_flags(entry, owner))
        for tok in it:
            if tok not in PATH_ARG_FLAGS and tok not in _LINK_ONLY_ARG_FLAGS:
                flags.append((tok,))
                continue
            arg = next(it, None)
            # A path never starts with "-"; that is the next flag
            if arg is None or (tok in PATH_ARG_FLAGS and arg.startswith("-")):
                raise EsphomeError(
                    f"{owner} build flags have '{tok}' with no argument; write "
                    f"the flag and its argument as one entry"
                )
            flags.append((tok, arg))
    return flags


def split_flags(flags: list[Flag]) -> tuple[list[Flag], list[Flag]]:
    """Route build flags to the compile and link lines."""
    compile_flags: list[Flag] = []
    link_flags: list[Flag] = []
    for flag in flags:
        name = flag[0]
        if len(flag) > 1:
            (compile_flags if name in PATH_ARG_FLAGS else link_flags).append(flag)
        elif name.startswith(_LINK_ONLY_PREFIXES):
            # Checked before the compile prefixes: -Wl, would match -W
            link_flags.append(flag)
        elif name.startswith(_COMPILE_ONLY_PREFIXES):
            compile_flags.append(flag)
        else:
            # -g, -O, -f*, -m*, -pthread, --coverage: both lines, as SCons
            compile_flags.append(flag)
            link_flags.append(flag)
    return compile_flags, link_flags


def _is_std(flag: Flag) -> bool:
    return flag[0].startswith("-std=")


def _is_cxx_std(flag: Flag) -> bool:
    return _is_std(flag) and "++" in flag[0]


def _anchored_flags(entries: Iterable[str], owner: str) -> list[Flag]:
    build_path = Path(CORE.build_path)
    return [
        anchor_path_flag(flag, build_path)
        for flag in parse_flags(sorted(entries), owner)
    ]


def _flag_lists() -> tuple[list[str], list[str], list[str]]:
    """The C, C++, and link flag lists (raw tokens), build_unflags applied.

    ``cg.set_cpp_standard`` wins over any ``-std=`` in the build flags for
    C++ compiles, as PlatformIO's unflag of every other standard did; C
    compiles never see a C++ standard.
    """
    # The funnel warns and drops empty glued arguments (-D "") itself
    compile_flags, link_flags = split_flags(
        _anchored_flags(CORE.build_flags, "esphome")
    )
    cflags = [f for f in compile_flags if not _is_cxx_std(f)]
    cxx_std = CORE.cpp_standard
    cxxflags = [f for f in compile_flags if not (cxx_std and _is_std(f))]
    if cxx_std:
        cxxflags.insert(0, (f"-std={cxx_std}",))
    cxxflags += [(tok,) for tok in get_project_cxx_compile_flags()]

    # A flag is removed whole, with its argument, as PlatformIO did
    unflags = set(_anchored_flags(CORE.build_unflags, "esphome build_unflags"))
    # An unflag that hits nothing (a typo, or -DUSE_FOO against
    # -DUSE_FOO=1) must be visible, since the user believes the flag is
    # gone while it still drives the build
    if unmatched := sorted(unflags - set(cflags) - set(cxxflags) - set(link_flags)):
        _LOGGER.warning(
            "build_unflags entries matched no build flag: %s",
            ", ".join(" ".join(flag) for flag in unmatched),
        )

    def keep(flags: list[Flag]) -> list[str]:
        return [tok for flag in flags if flag not in unflags for tok in flag]

    return keep(cflags), keep(cxxflags), keep(link_flags)


def _resolve_host_libraries() -> list[ArduinoLibrary]:
    """Every ``cg.add_library()`` entry, fetched from the registry.

    The host has no framework, so nothing is bundled and no framework
    compatibility check applies; the platform check keeps the strict
    manifest gate PlatformIO's native platform enforced. Manifest-less
    libraries (a bare git checkout) build with PlatformIO's default
    layout, as they did under its native platform.
    """
    if not CORE.platformio_libraries:
        return []
    from esphome.arduino.library import resolve_libraries

    return resolve_libraries(
        None,
        pio_platform=PIO_PLATFORM,
        board_mcu="host",
        cache_key=LIBRARY_CACHE_KEY,
        framework=None,
        manifest_optional=True,
    )


def _file_macro_maps(build_dir: Path) -> list[str]:
    """Flags that keep ``__FILE__`` relative to the build path.

    PlatformIO compiled ``src/x.cpp`` from the build path, and tools name
    things after that spelling (CodSpeed's benchmark ids). Here a source
    reaches the compiler by its absolute path, or relative to the build
    directory when ccache rewrites it.
    """
    build_path = Path(CORE.build_path)
    prefixes = (build_path, Path(os.path.relpath(build_path, build_dir)))
    return [
        shell_token(f"-fmacro-prefix-map={prefix}{os.sep}=", force=True)
        for prefix in prefixes
    ]


def _compiler_version(cxx: tuple[str, ...]) -> str:
    """What the compiler says it is: its path can stay the same across an
    update (the macOS shims in /usr/bin)."""
    result = subprocess.run(
        [*cxx, "--version"], capture_output=True, text=True, check=False
    )
    return result.stdout


def write_project(compilers: HostCompilers, ccache: str | None) -> bool:
    """Write the ninja build for the current configuration.

    ``ccache`` is the caller's already-resolved binary (None when disabled).
    Returns True when ``build.ninja`` changed, so the caller can skip work
    derived purely from it (the compile database) on unchanged builds.
    """
    build_dir = get_build_dir()
    mkdir_p(build_dir)
    src_dir = CORE.relative_src_path()
    if not src_dir.is_dir():
        # Generated project state, not install state: clean-all would not help
        raise EsphomeError(f"Generated source directory {src_dir} is missing")

    cflags, cxxflags, link_flags = _flag_lists()
    libraries = _resolve_host_libraries()

    include_dirs = [src_dir]
    for lib in libraries:
        include_dirs += lib.include_dirs
    includes = [f"-I{_q(d)}" for d in include_dirs]
    includes += _file_macro_maps(build_dir)

    # SCons's link line: $LINKFLAGS $SOURCES $_LIBDIRFLAGS $_LIBFLAGS, so
    # -L and -l trail the objects while every other link token leads
    lib_dirs = [Path(t[2:]) for t in link_flags if t.startswith("-L")]
    libs = [t for t in link_flags if t.startswith("-l")]
    linkflags = [shell_token(t) for t in link_flags if not t.startswith(("-L", "-l"))]
    for lib in libraries:
        lib_dirs += lib.link_dirs
        libs += [f"-l{name}" for name in lib.link_libs]
        linkflags += [shell_token(f) for f in lib.link_flags]

    # PlatformIO's ASPPCOM passes only -D/-I user flags to assembly
    asflags = [t for t in cflags if t.startswith(("-D", "-I"))]

    lines = [
        *tool_lines(compilers.cc, compilers.cxx, ccache),
        *compile_rule_lines(ccache),
        *pch_rule_lines(),
        "rule link",
        "  command = $cxx -o $out $linkflags @$out.rsp $archives $libdirflags $libflags",
        "  rspfile = $out.rsp",
        "  rspfile_content = $in_newline",
        "  description = LINK $out",
    ]
    if any(lib.sources and lib.lib_archive for lib in libraries):
        # Resolved only when an archive is built, so a system without
        # binutils still links a library-free configuration
        lines += ar_rule_lines(find_tool("AR", ("ar",)))
    lines += [
        "",
        f"cflags = {' '.join([*map(shell_token, cflags), *includes])}",
        f"cxxflags = {' '.join([*map(shell_token, cxxflags), *includes])}",
        f"asflags = {' '.join([*map(shell_token, asflags), *includes])}",
        f"linkflags = {' '.join(linkflags)}",
        f"libdirflags = {' '.join(f'-L{_q(d)}' for d in lib_dirs)}",
        f"libflags = {' '.join(shell_token(lib) for lib in libs)}",
        "",
    ]

    archives, direct_objs = library_edges(lines, libraries)

    src_cxx_override = pch_edges(
        lines,
        build_dir,
        src_dir,
        PCH_DEFAULT_HEADERS,
        # The arguments of a CXX override come before the flags
        [*compilers.cxx[1:], *cxxflags],
        (),
        (compilers.cxx[0], _compiler_version(compilers.cxx)) if pch_enabled() else (),
        compilers.cxx,
    )
    src_objs = compile_edges(
        lines,
        collect_sources(src_dir),
        src_dir,
        "src",
        cxx_override=src_cxx_override,
    )
    if not src_objs:
        raise EsphomeError(f"No source files found under {src_dir}")

    # Archives are not topologically sorted; GNU ld needs the group to
    # resolve references between them. ld64 loads archives iteratively and
    # rejects the option, so macOS lists them bare.
    archive_tokens = [shell_token(a) for a in archives]
    if archive_tokens and sys.platform != "darwin":
        archive_tokens = ["-Wl,--start-group", *archive_tokens, "-Wl,--end-group"]
    lines.append(
        f"build {PROGRAM_NAME}: link {' '.join(src_objs + direct_objs)} | "
        f"{' '.join(_e(a) for a in archives)}"
    )
    lines.append(f"  archives = {' '.join(archive_tokens)}")
    lines.append(f"default {PROGRAM_NAME}")
    lines.append("")

    return write_file_if_changed(build_dir / "build.ninja", "\n".join(lines))
