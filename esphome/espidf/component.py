"""ESP-IDF backend for the shared PlatformIO library converter.

The toolchain-agnostic resolution/download/caching pipeline lives in
``esphome.platformio.library``; this module only adds the ESP-IDF specifics:
emitting an ``idf_component_register`` ``CMakeLists.txt`` + ``idf_component.yml``
for each resolved library, running any PlatformIO ``extraScript``, and the
ESP-IDF platform/framework compatibility defaults.
"""

import logging
import os
from pathlib import Path

from esphome.core import CORE, Library
from esphome.espidf import variant_to_idf_target
from esphome.helpers import write_file_if_changed
from esphome.platformio.library import (
    DEFAULT_BUILD_FLAGS,
    DEFAULT_BUILD_INCLUDE_DIR,
    DEFAULT_BUILD_SRC_FILTER,
    ESPHOME_DATA_EXTRA_CMAKE_KEY,
    ESPHOME_DATA_KEY,
    ESPHOME_DATA_LINK_FLAGS_KEY,
    SRC_FILE_EXTENSIONS,
    ConvertedLibrary as IDFComponent,
    LibraryBackend,
    PathType,
    collect_filtered_files,
    convert_libraries,
    ensure_list,
    lex_build_flags,
    split_list_by_condition,
)

_LOGGER = logging.getLogger(__name__)

ESP32_PLATFORM = "espressif32"


def _idf_framework() -> str:
    """The framework token an ESP-IDF library manifest is expected to declare."""
    return "arduino" if CORE.using_arduino else "espidf"


def generate_cmakelists_txt(component: IDFComponent) -> str:
    """
    Generate a CMakeLists.txt file for an ESP-IDF component.

    This function creates the necessary CMake configuration to build a library
    with ESP-IDF, including source files, include directories, dependencies,
    and build flags.

    Args:
        component: The IDFComponent object containing library metadata and path

    Returns:
        str: The complete CMakeLists.txt content as a string
    """

    def escape_entry(p: PathType) -> str:
        # In CMakeLists.txt, backslashes and embedded quotes need escaping
        # (a quoted define value reaches here via the shlex round-trip)
        escaped = str(p).replace("\\", "\\\\").replace('"', '\\"')
        return f'"{escaped}"'

    def escape_path(p: PathType) -> str:
        # CMake uses forward slashes for paths on every platform and treats
        # backslashes as escape characters. On Windows os.path.relpath yields
        # backslash paths, which break CMake's list re-parsing (e.g. "\b" in
        # "src\backend" is an invalid character escape). Emit forward slashes,
        # which Windows accepts too, so the generated CMakeLists is portable.
        return f'"{str(p).replace(os.sep, "/")}"'

    # The library's own files live in source_path (the user's directory for a
    # local library, the downloaded dir otherwise). When it differs from the
    # component dir the CMakeLists must reference sources by absolute path.
    read_path = component.source_dir
    external = read_path.resolve() != component.path.resolve()

    # Extract the values
    build_src_dir = component.data.get("build", {}).get("srcDir", None)
    if not build_src_dir:
        for d in ["src", "Src", "."]:
            if (read_path / Path(d)).is_dir():
                build_src_dir = d
                break

    build_include_dir = component.data.get("build", {}).get(
        "includeDir", DEFAULT_BUILD_INCLUDE_DIR
    )
    build_src_filter = ensure_list(
        component.data.get("build", {}).get("srcFilter", DEFAULT_BUILD_SRC_FILTER)
    )
    # PlatformIO shell-lexes each build.flags entry; bare -I/-L/-l/-D tokens
    # re-glue to their argument so the prefix classifiers below route them.
    build_flags = lex_build_flags(
        component.data.get("build", {}).get("flags", DEFAULT_BUILD_FLAGS),
        f"library {component.name}",
    )

    # List all sources files
    build_src_files = collect_filtered_files(
        read_path / Path(build_src_dir), build_src_filter
    )

    # Only bake library.json-declared deps here. Project-managed and
    # built-in components come in via ${ESPHOME_PROJECT_MANAGED_COMPONENTS}
    # / ${ESPHOME_PROJECT_BUILTIN_COMPONENTS} set in the top-level
    # CMakeLists, so this file stays project-agnostic when shared from
    # the pio_components cache.
    requires: set[str] = {
        dependency.get_require_name() for dependency in component.dependencies
    }

    # Only keep sources. Reference them absolutely when they live outside the
    # component dir (a local library), relative otherwise.
    if external:
        build_src_files = [str(Path(p).resolve()) for p in build_src_files]
    else:
        build_src_files = [os.path.relpath(p, component.path) for p in build_src_files]
    build_src_files = [
        f for f in build_src_files if Path(f).suffix in SRC_FILE_EXTENSIONS
    ]

    # Handle build flags
    include_dir_flags, build_flags = split_list_by_condition(
        build_flags, lambda a: a[2:].strip() if a.startswith("-I") else None
    )
    link_directories, build_flags = split_list_by_condition(
        build_flags, lambda a: a[2:].strip() if a.startswith("-L") else None
    )
    link_libraries, build_flags = split_list_by_condition(
        build_flags, lambda a: a[2:].strip() if a.startswith("-l") else None
    )
    # A local library's relative -L paths are relative to its own directory;
    # resolve them against it so they still work from the component cache dir.
    # (read_path / d yields d unchanged when d is already absolute.)
    if external:
        link_directories = [
            str((read_path / Path(d)).resolve()) for d in link_directories
        ]

    # Split include directories from build_flags
    # Only keep an include directory if it exists
    build_include_dirs = [build_include_dir, build_src_dir] + include_dir_flags
    build_include_dirs = [
        d for d in build_include_dirs if (read_path / Path(d)).is_dir()
    ]
    if external:
        build_include_dirs = [
            str((read_path / Path(d)).resolve()) for d in build_include_dirs
        ]

    # Split build_flags list into private and public lists
    private_build_flags, public_build_flags = split_list_by_condition(
        build_flags, lambda a: a if a.startswith("-W") else None
    )

    # Generate the component
    content = "idf_component_register(\n"
    if build_src_files:
        str_srcs = " ".join([escape_path(p) for p in sorted(build_src_files)])
        content += f"  SRCS {str_srcs}\n"
    if build_include_dirs:
        str_include_dirs = " ".join([escape_path(p) for p in build_include_dirs])
        content += f"  INCLUDE_DIRS {str_include_dirs}\n"
    # Project-managed and built-in component lists are set per-project
    # via idf_build_set_property in the top-level CMakeLists; expanded
    # here at configure time. Keeping them out of the per-lib REQUIRES
    # means this CMakeLists is project-agnostic and reusable from the
    # pio_components cache across builds.
    str_requires = " ".join(
        [
            *sorted(requires),
            "${ESPHOME_PROJECT_MANAGED_COMPONENTS}",
            "${ESPHOME_PROJECT_BUILTIN_COMPONENTS}",
        ]
    )
    content += f"  REQUIRES {str_requires}\n"
    content += ")\n"

    # Add public and private build flags
    if public_build_flags:
        content += "target_compile_options(${COMPONENT_LIB} PUBLIC\n"
        for build_flag in public_build_flags:
            str_build_flag = escape_entry(build_flag)
            content += f"  {str_build_flag}\n"
        content += ")\n"
    if private_build_flags:
        content += "target_compile_options(${COMPONENT_LIB} PRIVATE\n"
        for build_flag in private_build_flags:
            str_build_flag = escape_entry(build_flag)
            content += f"  {str_build_flag}\n"
        content += ")\n"

    # Add library paths and files
    if link_directories:
        content += "target_link_directories(${COMPONENT_LIB} INTERFACE\n"
        for link_directory in link_directories:
            str_build_flag = escape_path(link_directory)
            content += f"  {str_build_flag}\n"
        content += ")\n"

    if link_libraries:
        content += "target_link_libraries(${COMPONENT_LIB} INTERFACE\n"
        for link_library in link_libraries:
            str_build_flag = escape_entry(link_library)
            content += f"  {str_build_flag}\n"
        content += ")\n"

    # Extra-script LINKFLAGS: routed to the link line; in
    # target_compile_options they would be silently ineffective
    if link_flags := component.data.get(ESPHOME_DATA_KEY, {}).get(
        ESPHOME_DATA_LINK_FLAGS_KEY, []
    ):
        content += "target_link_options(${COMPONENT_LIB} INTERFACE\n"
        for link_flag in link_flags:
            content += f"  {escape_entry(link_flag)}\n"
        content += ")\n"

    # Add custom CMake scripts
    content += "\n".join(
        component.data.get(ESPHOME_DATA_KEY, {}).get(ESPHOME_DATA_EXTRA_CMAKE_KEY, [])
    )

    return content


def generate_idf_component_yml(component: IDFComponent) -> str:
    """
    Generate ESP-IDF component YAML configuration for a library.

    Args:
        component: IDFComponent object to generate YAML for

    Returns:
        YAML string representation of ESP-IDF component configuration
    """
    from esphome import yaml_util

    data = {}

    # Metadata only: tolerate malformed shapes instead of crashing on a
    # third-party manifest (repository may legally be {"url": ...} or a
    # plain URL string)
    description = component.data.get("description")
    if isinstance(description, str) and description:
        data["description"] = description

    repository = component.data.get("repository")
    if isinstance(repository, dict):
        repository = repository.get("url")
    if isinstance(repository, str) and repository:
        data["repository"] = repository

    for dependency in component.dependencies:
        # Initialize dependencies section if needed
        if "dependencies" not in data:
            data["dependencies"] = {}

        # Every dependency has been resolved and downloaded before this runs,
        # so .path is always set.
        data["dependencies"][dependency.get_sanitized_name()] = {
            "override_path": str(dependency.path),
        }

    return yaml_util.dump(data)


def _emit_idf_component(component: IDFComponent) -> None:
    """Write the ESP-IDF build files for a resolved library into its cache dir."""
    from esphome.components.esp32 import get_esp32_variant
    from esphome.platformio.extra_script import apply_extra_script

    apply_extra_script(
        component,
        board_mcu=lambda: variant_to_idf_target(get_esp32_variant()),
        pio_platform="espressif32",
    )
    write_file_if_changed(
        component.path / "CMakeLists.txt",
        generate_cmakelists_txt(component),
    )
    write_file_if_changed(
        component.path / "idf_component.yml",
        generate_idf_component_yml(component),
    )


def generate_idf_components(libraries: list[Library]) -> list[IDFComponent]:
    """Resolve and convert a batch of PlatformIO libraries to IDF components."""
    backend = LibraryBackend(
        platform=ESP32_PLATFORM,
        framework=_idf_framework(),
        emit=_emit_idf_component,
        cache_key="idf",
    )
    return convert_libraries(libraries, backend)
