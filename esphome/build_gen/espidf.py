"""ESP-IDF direct build generator for ESPHome."""

import json
import logging
from pathlib import Path

from esphome.components.esp32 import (
    get_esp32_variant,
    get_excluded_builtin_components,
    get_managed_component_require_names,
    idf_version,
)
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.espidf import variant_to_idf_target
from esphome.framework_helpers import (
    get_project_compile_flags,
    get_project_cxx_compile_flags,
    get_project_link_flags,
)
from esphome.helpers import mkdir_p, write_file_if_changed

_LOGGER = logging.getLogger(__name__)

# Replaces the IDF default C++ standard (-std=gnu++2b appended to
# CXX_COMPILE_OPTIONS by project.cmake's __build_init) with the one set via
# cg.set_cpp_standard(). Emitted between include(project.cmake) and project(),
# i.e. after IDF appends its default and before the options are consumed, and
# applies project-wide like PlatformIO build_unflags.
CPP_STANDARD_TEMPLATE = """\
idf_build_get_property(esphome_cxx_compile_options CXX_COMPILE_OPTIONS)
list(FILTER esphome_cxx_compile_options EXCLUDE REGEX "^-std=")
list(APPEND esphome_cxx_compile_options "-std={standard}")
idf_build_set_property(CXX_COMPILE_OPTIONS "${{esphome_cxx_compile_options}}")"""


def get_available_components() -> list[str] | None:
    """List the built-in ESP-IDF components from ``project_description.json``.

    Only components below its ``idf_path/components`` count, which leaves out
    ``src``, IDF-managed components, converted PIO libs and project local
    ones such as the Arduino ``component_stubs``. Returns ``None`` if the
    build dir or ``project_description.json`` isn't ready yet.
    """
    if CORE.build_path is None:
        return None
    project_desc = Path(CORE.build_path) / "build" / "project_description.json"
    if not project_desc.exists():
        return None

    try:
        with project_desc.open(encoding="utf-8") as f:
            data = json.load(f)
        root = (Path(data["idf_path"]) / "components").resolve()
        result = [
            name
            for name, info in data.get("build_component_info", {}).items()
            if (comp_dir := info.get("dir"))
            and Path(comp_dir).resolve().is_relative_to(root)
        ]
    except (json.JSONDecodeError, KeyError, OSError) as err:
        _LOGGER.debug("Could not read %s: %s", project_desc, err)
        return None
    if not result:
        _LOGGER.warning("No ESP-IDF components found under %s", root)
    return result


def has_discovered_components() -> bool:
    """Check if a previous configure discovered any built-in components."""
    return bool(get_available_components())


def _cmake_quote(value: str) -> str:
    """Quote a cmake arg value for a set() line. add_cmake_arg rejects
    whitespace, quotes, and '$', so only backslashes need escaping."""
    escaped = value.replace("\\", "\\\\")
    return f'"{escaped}"'


def get_project_cmakelists(
    minimal: bool = False, builtin_components: list[str] | None = None
) -> str:
    """Generate the top-level CMakeLists.txt for ESP-IDF project.

    When ``minimal`` is true, omit ``ESPHOME_PROJECT_BUILTIN_COMPONENTS``
    since ``project_description.json`` may be stale on the first write.
    ``builtin_components`` supplies the discovered list (from the cache)
    instead of reading it from ``project_description.json``.
    """
    idf_target = variant_to_idf_target(get_esp32_variant())

    # esp_idf_size 2.x (bundled with IDF >=6.0) made NG the default and
    # removed the --ng flag; on 1.x (IDF 5.5) --ng is required to get
    # --format=raw because the legacy mode doesn't support it.
    size_ng_flag = "--ng" if idf_version() < cv.Version(6, 0, 0) else ""

    # Project-wide compile options: -D defines and -W warning flags (skip
    # -Wl, linker flags — those go on the src component via
    # target_link_options below). Emitted via idf_build_set_property so the
    # flags propagate to every IDF component (including managed ones like
    # esphome__micro-mp3) rather than just src/. Required so suppressions
    # like ``-Wno-error=maybe-uninitialized`` actually silence warnings in
    # third-party components we don't author.
    project_compile_opts = get_project_compile_flags()
    extra_compile_options = "\n".join(
        f'idf_build_set_property(COMPILE_OPTIONS "{flag}" APPEND)'
        for flag in project_compile_opts
    )

    # Flags registered via cg.add_cxx_build_flag() go on CXX_COMPILE_OPTIONS
    # (not COMPILE_OPTIONS) because GCC warns when a C++-only flag such as
    # -Wno-volatile is passed on a C compile.
    cxx_compile_options = "\n".join(
        f'idf_build_set_property(CXX_COMPILE_OPTIONS "{flag}" APPEND)'
        for flag in get_project_cxx_compile_flags()
    )

    cpp_standard_options = (
        CPP_STANDARD_TEMPLATE.format(standard=CORE.cpp_standard)
        if CORE.cpp_standard
        else ""
    )

    # CMake variables registered via cg.add_cmake_arg(). Emitted before
    # include(project.cmake) so values like EXCLUDE_COMPONENTS are already
    # set when project.cmake seeds the component list, and on minimal
    # (discovery) writes too so excluded components never register.
    cmake_args = "\n".join(
        f"set({name} {_cmake_quote(value)})"
        for name, value in sorted(CORE.cmake_args.items())
    )

    # Per-project list exposed as a CMake variable so converted PIO libs
    # can reference ${ESPHOME_PROJECT_MANAGED_COMPONENTS} without baking
    # project-specific names into their cached CMakeLists.
    #
    # Emit via idf_build_set_property (not plain set()) so the value is
    # serialised into build_properties.temp.cmake and visible to IDF's
    # early requirements-expansion pass (component_get_requirements.cmake
    # runs as a separate CMake script invocation that doesn't load the
    # project's top-level CMakeLists; without this, ${ESPHOME_PROJECT_
    # MANAGED_COMPONENTS} in a converted-lib REQUIRES expands to empty).
    managed_components_property = "\n".join(
        f"idf_build_set_property(ESPHOME_PROJECT_MANAGED_COMPONENTS {name} APPEND)"
        for name in get_managed_component_require_names()
    )

    # Built-in IDF components exposed via our own property (not IDF's
    # __COMPONENT_REQUIRES_COMMON, which would append them to every
    # component's REQUIRES including real IDF components). Referenced by
    # src/CMakeLists and by each converted PIO lib's CMakeLists. Skipped
    # on minimal writes because project_description.json may be stale.
    # Excluded components are dropped here as well: a stale
    # project_description.json from a build without exclusions may still
    # list them, and requiring an excluded component pulls it back into
    # the build (IDF requirement expansion overrides EXCLUDE_COMPONENTS).
    # Derived from the EXCLUDE_COMPONENTS cmake arg emitted above so the
    # two can never disagree within one generated file.
    builtin_components_property = (
        ""
        if minimal
        else "\n".join(
            f"idf_build_set_property(ESPHOME_PROJECT_BUILTIN_COMPONENTS {name} APPEND)"
            for name in sorted(
                set(
                    builtin_components
                    if builtin_components is not None
                    else get_available_components() or []
                ).difference(CORE.cmake_args.get("EXCLUDE_COMPONENTS", "").split(";"))
            )
        )
    )

    return f"""\
# Auto-generated by ESPHome
cmake_minimum_required(VERSION 3.16)

# On Windows, Ninja can fail with:
#   "CreateProcess: The parameter is incorrect (is the command line too long?)"
# when compiler/linker command lines exceed the OS length limit.
#
# The following settings force CMake/Ninja to use *response files* (@file.rsp)
# to pass long lists of includes, objects, and other arguments indirectly,
# avoiding command-line length limits and fixing the build failure.
#
# This is especially useful for large ESP-IDF / ESPHome projects with many
# source files or include directories.
set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES 1)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES 1)
set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS 1)
set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS 1)
set(CMAKE_NINJA_FORCE_RESPONSE_FILE 1)

set(IDF_TARGET {idf_target})
set(EXTRA_COMPONENT_DIRS ${{CMAKE_SOURCE_DIR}}/src)

{cmake_args}

include($ENV{{IDF_PATH}}/tools/cmake/project.cmake)

{cpp_standard_options}

{cxx_compile_options}

{extra_compile_options}

{managed_components_property}

{builtin_components_property}

project({CORE.name})

# Emit raw JSON size data for ESPHome to read post-build.
add_custom_command(
    TARGET ${{CMAKE_PROJECT_NAME}}.elf POST_BUILD
    COMMAND ${{PYTHON}} -m esp_idf_size {size_ng_flag} --format=raw
            -o ${{CMAKE_BINARY_DIR}}/esp_idf_size.json
            ${{CMAKE_PROJECT_NAME}}.map
    WORKING_DIRECTORY ${{CMAKE_BINARY_DIR}}
    VERBATIM
)
"""


def get_component_cmakelists() -> str:
    """Generate the main component CMakeLists.txt.

    REQUIRES pulls in the discovered built-in IDF components via the
    project-level variables set in the top-level CMakeLists.
    """
    # Extract linker options (-Wl, flags). Compile flags (-D, -W) are
    # emitted project-wide via idf_build_set_property in
    # get_project_cmakelists so they reach every component, not just src/.
    link_opts = get_project_link_flags()
    link_opts_str = "\n    ".join(link_opts) if link_opts else ""

    return f"""\
# Auto-generated by ESPHome
# CONFIGURE_DEPENDS asks CMake to re-check the glob each build so test
# runs that reuse the build dir don't compile stale source paths. It's
# invalid in script mode (cmake -P), which is how IDF's
# component_get_requirements.cmake includes us, so skip it there.
if(CMAKE_SCRIPT_MODE_FILE)
    file(GLOB_RECURSE app_sources
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.cpp"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.cc"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.cxx"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.c++"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.c"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.cpp"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.cc"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.cxx"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.c++"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.c"
    )
else()
    file(GLOB_RECURSE app_sources CONFIGURE_DEPENDS
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.cpp"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.cc"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.cxx"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.c++"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/*.c"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.cpp"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.cc"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.cxx"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.c++"
        "${{CMAKE_CURRENT_SOURCE_DIR}}/esphome/*.c"
    )
endif()

idf_component_register(
    SRCS ${{app_sources}}
    INCLUDE_DIRS "." "esphome"
    REQUIRES ${{ESPHOME_PROJECT_BUILTIN_COMPONENTS}}
)

# ESPHome linker options
target_link_options(${{COMPONENT_LIB}} PUBLIC
    {link_opts_str}
)
"""


def write_project(
    minimal: bool = False, builtin_components: list[str] | None = None
) -> None:
    """Write ESP-IDF project files."""
    mkdir_p(CORE.build_path)
    mkdir_p(CORE.relative_src_path())

    # Write top-level CMakeLists.txt
    write_file_if_changed(
        CORE.relative_build_path("CMakeLists.txt"),
        get_project_cmakelists(minimal=minimal, builtin_components=builtin_components),
    )

    # Write component CMakeLists.txt in src/
    write_file_if_changed(
        CORE.relative_src_path("CMakeLists.txt"),
        get_component_cmakelists(),
    )

    # Snapshot the exclusion set so has_outdated_files() can trigger a
    # discovery reconfigure when it changes. Excluded components never
    # register in project_description.json, so re-including one (e.g. a
    # config gains mqtt) requires a fresh discovery pass before the
    # ESPHOME_PROJECT_BUILTIN_COMPONENTS property can list it.
    write_file_if_changed(
        CORE.relative_build_path("exclude_components.esphomeinternal"),
        ";".join(get_excluded_builtin_components()),
    )
