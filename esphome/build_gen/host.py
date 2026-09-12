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

import logging
from pathlib import Path
import sys
from typing import TYPE_CHECKING

from esphome.build_helpers.ninja import (
    escape as _e,
    quote_path as _q,
    shell_token as _shell_token,
)
from esphome.core import CORE, EsphomeError
from esphome.framework_helpers import (
    get_project_cxx_compile_flags,
    strip_win_long_path_prefix,
)
from esphome.helpers import mkdir_p, write_file_if_changed
from esphome.host.toolchain import PROGRAM_NAME, HostCompilers, find_tool, get_build_dir
from esphome.platformio.library import SOURCE_KIND_FOR_SUFFIX, lex_build_flags

if TYPE_CHECKING:
    from esphome.arduino.library import ArduinoLibrary

_LOGGER = logging.getLogger(__name__)

# The PlatformIO platform the host built under; registry manifests declare
# compatibility against it, as lib_compat_mode=strict checked before
PIO_PLATFORM = "native"
# Namespaces the shared library download cache (pio_components/host/)
LIBRARY_CACHE_KEY = "host"

# Flag shapes that only the compiler understands; dropped from the link line
_COMPILE_ONLY_PREFIXES = ("-D", "-U", "-I", "-std=", "-W")
# Compile-only flags whose argument is the next token
_COMPILE_ONLY_ARG_FLAGS = ("-include", "-imacros", "-isystem", "-iquote", "-idirafter")
# Flag shapes that only the linker consumes; inert on a -c compile line
_LINK_ONLY_PREFIXES = ("-l", "-L", "-Wl,")
# Link-only flags whose argument is the next token (macOS frameworks)
_LINK_ONLY_ARG_FLAGS = ("-framework",)


def split_flags(tokens: list[str]) -> tuple[list[str], list[str]]:
    """Route lexed build flags to the compile and link lines (raw tokens).

    Two-token flags travel with their argument so a stray ``foo.h`` never
    lands on the link line as an input file.
    """
    compile_flags: list[str] = []
    link_flags: list[str] = []
    it = iter(tokens)
    for tok in it:
        if tok in _COMPILE_ONLY_ARG_FLAGS or tok in _LINK_ONLY_ARG_FLAGS:
            arg = next(it, None)
            if arg is None:
                raise EsphomeError(
                    f"build_flags has a trailing '{tok}' with no argument"
                )
            target = compile_flags if tok in _COMPILE_ONLY_ARG_FLAGS else link_flags
            target += [tok, arg]
        elif tok.startswith(_LINK_ONLY_PREFIXES):
            # Checked before the compile prefixes: -Wl, would match -W
            link_flags.append(tok)
        elif tok.startswith(_COMPILE_ONLY_PREFIXES):
            compile_flags.append(tok)
        else:
            # -g, -O, -f*, -m*, -pthread, --coverage: both lines, as SCons
            compile_flags.append(tok)
            link_flags.append(tok)
    return compile_flags, link_flags


def _is_cxx_std(tok: str) -> bool:
    return tok.startswith("-std=") and "++" in tok


def _flag_lists() -> tuple[list[str], list[str], list[str]]:
    """The C, C++, and link flag lists (raw tokens), build_unflags applied.

    ``cg.set_cpp_standard`` wins over any ``-std=`` in the build flags for
    C++ compiles, as PlatformIO's unflag of every other standard did; C
    compiles never see a C++ standard.
    """
    # The funnel warns and drops empty glued arguments (-D "") itself
    tokens = lex_build_flags(sorted(CORE.build_flags), "esphome")
    compile_flags, link_flags = split_flags(tokens)
    cflags = [t for t in compile_flags if not _is_cxx_std(t)]
    cxx_std = CORE.cpp_standard
    cxxflags = [t for t in compile_flags if not (cxx_std and t.startswith("-std="))]
    if cxx_std:
        cxxflags.insert(0, f"-std={cxx_std}")
    cxxflags += get_project_cxx_compile_flags()

    unflags = set(lex_build_flags(sorted(CORE.build_unflags), "esphome build_unflags"))
    # Matching is whole-token; an unflag that hits nothing (a typo, or
    # -DUSE_FOO against -DUSE_FOO=1) must be visible, since the user
    # believes the flag is gone while it still drives the build
    if unmatched := sorted(unflags - set(cflags) - set(cxxflags) - set(link_flags)):
        _LOGGER.warning(
            "build_unflags entries matched no build flag: %s", ", ".join(unmatched)
        )
    return tuple(
        [t for t in flags if t not in unflags]
        for flags in (cflags, cxxflags, link_flags)
    )  # type: ignore[return-value]


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


def _collect_sources(root: Path) -> list[Path]:
    return sorted(p for p in root.rglob("*") if p.suffix in SOURCE_KIND_FOR_SUFFIX)


def _common_parent(paths: list[Path]) -> Path:
    import os

    return Path(os.path.commonpath([str(p.parent) for p in paths]))


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
        # statements (archive/link inputs), which use ninja escaping
        objects.append(escaped_obj)
    return objects


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

    # SCons's link line: $LINKFLAGS $SOURCES $_LIBDIRFLAGS $_LIBFLAGS, so
    # -L and -l trail the objects while every other link token leads
    lib_dirs = [Path(t[2:]) for t in link_flags if t.startswith("-L")]
    libs = [t for t in link_flags if t.startswith("-l")]
    linkflags = [_shell_token(t) for t in link_flags if not t.startswith(("-L", "-l"))]
    for lib in libraries:
        lib_dirs += lib.link_dirs
        libs += [f"-l{name}" for name in lib.link_libs]
        linkflags += [_shell_token(f) for f in lib.link_flags]

    # PlatformIO's ASPPCOM passes only -D/-I user flags to assembly
    asflags = [t for t in cflags if t.startswith(("-D", "-I"))]

    build_tool = Path(__file__).parent / "build_tool.py"

    # $in/$out stay unquoted: ninja escapes its built-in path variables
    # itself; only literal paths need _q().
    lines = [
        "# Auto-generated by ESPHome",
        "ninja_required_version = 1.5",
        f"cc = {_q(compilers.cc)}",
        f"cxx = {_q(compilers.cxx)}",
        # The NSIS launcher starts Python with a \\?\ extended-length path
        # that cmd.exe cannot spawn; same strip every other emitted binary
        # path gets
        f"python = {_q(strip_win_long_path_prefix(sys.executable))}",
        f"buildtool = {_q(build_tool)}",
        f"ccache = {_q(ccache) if ccache else ''}",
        "",
        # Rule names match SOURCE_KIND_FOR_SUFFIX values (c, cxx, asm, aspp)
        # and toolchain.COMPILE_RULES
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
        # Plain assembler, as SCons's ASCOM: no preprocessor, so no
        # depfile and no $flags (defines/includes) either
        "rule asm",
        "  command = $ccache $cc -x assembler $asflags -c $in -o $out",
        "  description = AS $out",
        "rule link",
        "  command = $cxx -o $out $linkflags @$out.rsp $archives $libdirflags $libflags",
        "  rspfile = $out.rsp",
        "  rspfile_content = $in_newline",
        "  description = LINK $out",
    ]
    if any(lib.sources and lib.lib_archive for lib in libraries):
        # Resolved only when an archive is built, so a system without
        # binutils still links a library-free configuration
        lines += [
            "rule ar",
            f"  command = $python $buildtool ar {_q(find_tool('AR', ('ar',)))} $out $out.rsp",
            "  rspfile = $out.rsp",
            "  rspfile_content = $in_newline",
            "  description = AR $out",
        ]
    lines += [
        "",
        f"cflags = {' '.join([*map(_shell_token, cflags), *includes])}",
        f"cxxflags = {' '.join([*map(_shell_token, cxxflags), *includes])}",
        f"asflags = {' '.join([*map(_shell_token, asflags), *includes])}",
        f"linkflags = {' '.join(linkflags)}",
        f"libdirflags = {' '.join(f'-L{_q(d)}' for d in lib_dirs)}",
        f"libflags = {' '.join(_shell_token(lib) for lib in libs)}",
        "",
    ]

    archives: list[str] = []
    direct_objs: list[str] = []
    for lib in libraries:
        if not lib.sources:
            # Header-only libraries are legitimate; the log makes an empty
            # srcFilter or broken tree traceable before link errors do
            _LOGGER.debug(
                "Library %s has no source files; contributing includes only",
                lib.name,
            )
            continue
        objs = _ninja_compile_edges(
            lines,
            lib.sources,
            _common_parent(lib.sources),
            f"lib/{lib.name}",
            flags=" ".join(_shell_token(f) for f in lib.flags),
        )
        if not lib.lib_archive:
            # libArchive: false: hand the objects to the linker directly so
            # unreferenced-but-required symbols (weak overrides) survive
            direct_objs.extend(objs)
            continue
        archive = f"lib{lib.name}.a"
        lines.append(f"build {_e(archive)}: ar {' '.join(objs)}")
        archives.append(archive)

    src_objs = _ninja_compile_edges(lines, _collect_sources(src_dir), src_dir, "src")
    if not src_objs:
        raise EsphomeError(f"No source files found under {src_dir}")

    # Archives are not topologically sorted; GNU ld needs the group to
    # resolve references between them. ld64 loads archives iteratively and
    # rejects the option, so macOS lists them bare.
    archive_tokens = [_shell_token(a) for a in archives]
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
