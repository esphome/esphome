"""Zephyr backend for the shared PlatformIO library converter.

For each PlatformIO library added via ``cg.add_library()``, emit a Zephyr
external module (``zephyr/module.yml`` + ``zephyr/CMakeLists.txt`` built with the
``zephyr_library*`` API) into the shared ``pio_components`` cache. The caller
wires the resulting module directories into the build via
``EXTRA_ZEPHYR_MODULES``; Zephyr then compiles each module and links it into the
final image.

Only framework-agnostic libraries (plain C/C++ that doesn't depend on the Arduino
API) will actually compile under Zephyr — this converter shares the
fetch/parse/cache plumbing, not API compatibility.
"""

from pathlib import Path

from esphome import yaml_util
from esphome.core import EsphomeError, Library
from esphome.helpers import write_file_if_changed
from esphome.platformio.library import (
    DEFAULT_BUILD_FLAGS,
    DEFAULT_BUILD_INCLUDE_DIR,
    DEFAULT_BUILD_SRC_FILTER,
    SRC_FILE_EXTENSIONS,
    ConvertedLibrary,
    LibraryBackend,
    PathType,
    collect_filtered_files,
    convert_libraries,
    ensure_list,
    split_list_by_condition,
)

# Zephyr libraries declare frameworks rarely and the PIO ``platforms`` token for
# nRF is seldom present, so the platform check is disabled (None) and only the
# framework mismatch warning fires.
ZEPHYR_FRAMEWORK = "zephyr"


def _escape(p: PathType) -> str:
    # In CMakeLists.txt, backslashes need to be escaped (mirrors the ESP-IDF
    # backend's escape_entry). Doubling -- rather than rewriting '\' -> '/' --
    # preserves content, so it's safe for arbitrary build flags (e.g. a -D value
    # containing a backslash) as well as Windows paths.
    return f'"{str(p)}"'.replace("\\", "\\\\")


def generate_module_yml(component: ConvertedLibrary) -> str:
    """Render the ``zephyr/module.yml`` manifest for a converted library."""
    return yaml_util.dump(
        {
            "name": component.get_require_name(),
            "build": {"cmake": "zephyr"},
        }
    )


def generate_cmakelists_txt(component: ConvertedLibrary) -> str:
    """Render the ``zephyr/CMakeLists.txt`` that builds a converted library.

    Sources/includes are emitted as absolute paths since the CMakeLists lives in
    the library's ``zephyr/`` subdir while its sources sit alongside it. Include
    dirs are published globally so the app (and sibling libraries) can include the
    library's headers, mirroring ESP-IDF's public ``INCLUDE_DIRS``.
    """
    build = component.data.get("build", {})

    build_src_dir = build.get("srcDir")
    if not build_src_dir:
        for d in ["src", "Src", "."]:
            if (component.path / Path(d)).is_dir():
                build_src_dir = d
                break

    build_include_dir = build.get("includeDir", DEFAULT_BUILD_INCLUDE_DIR)
    build_src_filter = ensure_list(build.get("srcFilter", DEFAULT_BUILD_SRC_FILTER))
    build_flags = ensure_list(build.get("flags", DEFAULT_BUILD_FLAGS))

    src_files = collect_filtered_files(
        component.path / Path(build_src_dir), build_src_filter
    )
    src_files = sorted(
        str(Path(p).resolve())
        for p in src_files
        if Path(p).suffix in SRC_FILE_EXTENSIONS
    )

    include_dir_flags, build_flags = split_list_by_condition(
        build_flags, lambda a: a[2:].strip() if a.startswith("-I") else None
    )
    link_directories, build_flags = split_list_by_condition(
        build_flags, lambda a: a[2:].strip() if a.startswith("-L") else None
    )
    link_libraries, build_flags = split_list_by_condition(
        build_flags, lambda a: a[2:].strip() if a.startswith("-l") else None
    )

    include_dirs = [build_include_dir, build_src_dir, *include_dir_flags]
    include_dirs = [
        str((component.path / Path(d)).resolve())
        for d in include_dirs
        if (component.path / Path(d)).is_dir()
    ]

    lines = [f"zephyr_library_named({component.get_require_name()})"]
    if src_files:
        lines += [
            "zephyr_library_sources(",
            *[f"  {_escape(p)}" for p in src_files],
            ")",
        ]
    if include_dirs:
        lines += [
            "zephyr_include_directories(",
            *[f"  {_escape(p)}" for p in include_dirs],
            ")",
        ]
    if build_flags:
        lines += [
            "zephyr_library_compile_options(",
            *[f"  {_escape(f)}" for f in build_flags],
            ")",
        ]
    # Best-effort link wiring; most Zephyr-portable libraries don't need it.
    link_flags = [f"-L{d}" for d in link_directories] + [
        f"-l{lib}" for lib in link_libraries
    ]
    if link_flags:
        lines += [
            "zephyr_link_libraries(",
            *[f"  {_escape(f)}" for f in link_flags],
            ")",
        ]

    return "\n".join(lines) + "\n"


def _emit_zephyr_module(component: ConvertedLibrary) -> None:
    zephyr_dir = component.path / "zephyr"
    write_file_if_changed(zephyr_dir / "module.yml", generate_module_yml(component))
    write_file_if_changed(
        zephyr_dir / "CMakeLists.txt", generate_cmakelists_txt(component)
    )


def generate_zephyr_modules(libraries: list[Library]) -> list[Path]:
    """Convert ``libraries`` to Zephyr modules and return all module directories.

    The returned list includes transitive dependencies (each converted library is
    its own module). Every directory should be added to ``EXTRA_ZEPHYR_MODULES``;
    Zephyr links all module libraries into the image, so cross-library symbols
    resolve without explicit dependency declarations.

    Raises ``EsphomeError`` if two libraries resolve to the same Zephyr module
    name -- each module's CMakeLists calls ``zephyr_library_named(<name>)``, so a
    duplicate would otherwise fail the build with a CMake "target already exists".
    The converter already warns when a library is referenced under inconsistent
    specs (bare ``name`` vs ``owner/name``, git vs registry); this turns that into
    an actionable error at the Zephyr boundary where it is fatal.
    """
    module_dirs: list[Path] = []
    by_name: dict[str, Path] = {}

    def emit(component: ConvertedLibrary) -> None:
        name = component.get_require_name()
        if name in by_name:
            raise EsphomeError(
                f"Two libraries resolve to the same Zephyr module '{name}' "
                f"({by_name[name]} and {component.path}). Reference the library "
                f"consistently (e.g. always as 'owner/name') so it resolves once."
            )
        by_name[name] = component.path
        _emit_zephyr_module(component)
        module_dirs.append(component.path)

    backend = LibraryBackend(
        platform=None, framework=ZEPHYR_FRAMEWORK, emit=emit, cache_key="zephyr"
    )
    convert_libraries(libraries, backend)
    return module_dirs
