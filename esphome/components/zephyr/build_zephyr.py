"""The native west build that nrf52 and the Zephyr toolchain share."""

import logging
from pathlib import Path

from esphome.build_helpers import pch
from esphome.build_helpers.ccache import ccache_env
from esphome.core import CORE
from esphome.framework_helpers import (
    cmake_escape,
    get_cmake_compile_flags,
    get_cmake_link_flags,
    lex_cmake_build_flags,
    quote_cmake_args,
)
from esphome.helpers import rmtree, write_file_if_changed

_LOGGER = logging.getLogger(__name__)

# GCC only loads a precompiled header ahead of every other forced header, and
# Zephyr forces two with -imacros. They hold macros only, so the C++ sources
# of the app get them through the precompiled header.
_PCH_CMAKE_LINES = [
    "",
    "# ESPHome precompiled header",
    "get_property(esphome_options TARGET zephyr_interface",
    "    PROPERTY INTERFACE_COMPILE_OPTIONS)",
    "set(esphome_kept_options)",
    "set(esphome_pch_headers)",
    "foreach(option IN LISTS esphome_options)",
    '  if(option MATCHES "imacros> ([^>]+)")',
    '    list(APPEND esphome_pch_headers "${CMAKE_MATCH_1}")',
    "    list(APPEND esphome_kept_options",
    '        "$<$<NOT:$<AND:$<COMPILE_LANGUAGE:CXX>,$<STREQUAL:$<TARGET_PROPERTY:NAME>,app>>>:${option}>")',
    "  else()",
    '    list(APPEND esphome_kept_options "${option}")',
    "  endif()",
    "endforeach()",
    "if(NOT esphome_pch_headers)",
    '  message(FATAL_ERROR "ESPHome: the headers Zephyr forces were not found, so "',
    '      "the precompiled header would not load (set ESPHOME_PCH_ENABLE=0)")',
    "endif()",
    "set_property(TARGET zephyr_interface",
    '    PROPERTY INTERFACE_COMPILE_OPTIONS "${esphome_kept_options}")',
    *(
        f'list(APPEND esphome_pch_headers "${{CMAKE_CURRENT_LIST_DIR}}/../src/{header}")'
        for header in pch.PCH_DEFAULT_HEADERS
    ),
    'list(TRANSFORM esphome_pch_headers REPLACE "(.+)" "$<$<COMPILE_LANGUAGE:CXX>:\\\\1>")',
    "target_precompile_headers(app PRIVATE ${esphome_pch_headers})",
]


def generate_cmake_lists(platform_name: str, pch_on: bool) -> bool:
    """Write the project CMakeLists.txt, returning True if it changed.
    ``platform_name`` names the platform in build flag warnings."""
    entries = lex_cmake_build_flags(platform_name)
    compile_flags = get_cmake_compile_flags(entries)
    link_flags = get_cmake_link_flags(entries)

    # Convert any PlatformIO libraries added via cg.add_library() into Zephyr
    # modules and discover them through EXTRA_ZEPHYR_MODULES (a CMake list, set
    # before find_package(Zephyr) so the modules are picked up). Only
    # framework-agnostic libraries actually compile under Zephyr.
    from .library import DEFINES_VAR, generate_zephyr_modules  # noqa: PLC0415

    module_dirs = generate_zephyr_modules(list(CORE.platformio_libraries.values()))
    defines = [flag for flag in compile_flags if flag.startswith("-D")]
    options = [flag for flag in compile_flags if not flag.startswith("-D")]

    lines = [
        "cmake_minimum_required(VERSION 3.20.0)",
        "",
        'set(Zephyr_DIR "$ENV{ZEPHYR_BASE}/share/zephyr-package/cmake/")',
        "",
    ]

    if module_dirs:
        modules = ";".join(str(d).replace("\\", "/") for d in module_dirs)
        lines += [f'set(EXTRA_ZEPHYR_MODULES "{modules}")', ""]
        if defines:
            escaped = ";".join(cmake_escape(flag) for flag in defines)
            lines += [f'set({DEFINES_VAR} "{escaped}")', ""]

    lines += [
        "find_package(Zephyr REQUIRED)",
        "",
        f"project({CORE.name})",
        "",
        'file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp" "${CMAKE_CURRENT_LIST_DIR}/../src/*.c")',
        "",
        "target_sources(app PRIVATE ${APP_SOURCES})",
        'target_include_directories(app PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../src")',
    ]

    if defines:
        lines += [
            "",
            "target_compile_options(app PRIVATE",
            *[f"  {arg}" for arg in quote_cmake_args(defines)],
            ")",
        ]
    if options:
        # A target's own options come before the ones it inherits from zephyr_interface, and the
        # compiler keeps the last of two opposing warning flags. Source options come after both.
        lines += [
            "",
            "set_source_files_properties(${APP_SOURCES} PROPERTIES",
            f'  COMPILE_OPTIONS "{";".join(cmake_escape(flag) for flag in options)}"',
            ")",
        ]

    if pch_on:
        lines += _PCH_CMAKE_LINES

    if link_flags:
        lines += [
            "",
            "zephyr_ld_options(",
            *[f"  {arg}" for arg in quote_cmake_args(link_flags)],
            ")",
        ]

    return write_file_if_changed(
        CORE.relative_build_path("zephyr", "CMakeLists.txt"),
        "\n".join(lines) + "\n",
    )


def add_ccache_env(
    env: dict[str, str], ccache: str | None, tools_cache: tuple[str, str]
) -> None:
    """Add the managed ccache settings to a west build's ``env``; ``ccache`` is the
    resolved binary (resolve_ccache_path), or None when ccache is disabled."""
    if ccache is None:
        # Zephyr wraps compiles with any ccache it finds; unmanaged it
        # must not cache (a sysbuild image never sees USE_CCACHE=0).
        env.setdefault("CCACHE_DISABLE", "1")
        return
    env.update(ccache_env(ccache, tools_cache))
    # Drop only the per build map entry (posix, CMake's spelling);
    # its from side covers no compiled sources. A spaced path cannot
    # survive ccache's space split list, so it stays hashed.
    source_dir = CORE.relative_build_path("zephyr").as_posix()
    if any(ch.isspace() for ch in source_dir):
        _LOGGER.debug("Whitespace in %s; the per build map stays hashed", source_dir)
    else:
        device_map = f"-fmacro-prefix-map={source_dir}=CMAKE_SOURCE_DIR"
        env["CCACHE_IGNOREOPTIONS"] = (
            f"{env.get('CCACHE_IGNOREOPTIONS', '')} {device_map}".strip()
        )


def clean_changed_build(build_dir: Path, cmake_lists_changed: bool) -> None:
    """A missing CMake cache (dropped by zephyr's copy_files() on config
    change) or a changed CMakeLists.txt requires a pristine build: Zephyr
    caches Kconfig/devicetree state that survives a plain cmake re-run.
    West can't do the wipe -- its pristine modes only recognize a build dir
    by reading ZEPHYR_BASE from the very cache that was dropped."""
    if (
        cmake_lists_changed or not (build_dir / "CMakeCache.txt").is_file()
    ) and build_dir.is_dir():
        _LOGGER.info("Build inputs changed, cleaning %s", build_dir)
        rmtree(build_dir)


def west_build_command(
    python_executable: Path,
    board: str,
    build_dir: Path,
    source_dir: Path,
) -> list[str]:
    return [
        str(python_executable),
        "-m",
        "west",
        "build",
        "--pristine=auto",
        "-b",
        board,
        "-d",
        str(build_dir),
        str(source_dir),
        "--",
        # Only adds -DNDEBUG (Kconfig sets the optimization level); picolibc used to force it
        "-DCMAKE_BUILD_TYPE=MinSizeRel",
    ]
