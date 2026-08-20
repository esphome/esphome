"""Arduino ESP8266 backend for the shared PlatformIO library converter.

Turns the libraries registered via ``cg.add_library()`` into build inputs for
the ninja generator. Bare names that exist under the framework's bundled
``libraries/`` directory (ESP8266WiFi, Wire, SPI, ...) are read straight from
the framework tree; everything else goes through the shared
resolution/download pipeline in ``esphome.platformio.library``.

Mirrors PlatformIO's ``lib_ldf_mode=off`` behavior: each library builds into
its own static archive and every library's include dir joins one global
include path.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import logging
from pathlib import Path
import shlex

from esphome.core import CORE, Library
from esphome.espidf.extra_script import apply_extra_script
from esphome.platformio.library import (
    DEFAULT_BUILD_INCLUDE_DIR,
    DEFAULT_BUILD_SRC_FILTER,
    SRC_FILE_EXTENSIONS,
    ConvertedLibrary,
    InvalidLibrary,
    LibraryBackend,
    check_library_data,
    collect_filtered_files,
    convert_libraries,
    ensure_list,
    normalize_dependencies,
    parse_library_properties,
)

_LOGGER = logging.getLogger(__name__)

ESP8266_PLATFORM = "espressif8266"


@dataclass
class ArduinoLibrary:
    """One resolved library, ready for the ninja generator."""

    name: str
    sources: list[Path] = field(default_factory=list)
    include_dirs: list[Path] = field(default_factory=list)
    # Extra compile flags private to this library's own sources
    flags: list[str] = field(default_factory=list)
    # Link inputs the library contributes (-L dirs / -l libs, e.g. from
    # precompiled vendor blobs) and -Wl, options for the firmware link
    link_dirs: list[Path] = field(default_factory=list)
    link_libs: list[str] = field(default_factory=list)
    link_flags: list[str] = field(default_factory=list)


def _library_info(name: str, read_path: Path, data: dict) -> ArduinoLibrary:
    """Resolve one library's sources, include dirs, and flags (PIO semantics)."""
    build = data.get("build", {})

    # PIO's source-dir resolution: manifest srcDir, else src/Src, else the root
    src_dir = build.get("srcDir") or next(
        (d for d in ("src", "Src") if (read_path / d).is_dir()), "."
    )

    src_filter = ensure_list(build.get("srcFilter", DEFAULT_BUILD_SRC_FILTER))
    # PlatformIO shell-lexes each build.flags entry
    raw_flags = [
        token
        for entry in ensure_list(build.get("flags", []))
        for token in shlex.split(entry)
    ]

    lib = ArduinoLibrary(name=name)
    it = iter(raw_flags)
    include_flags: list[str] = []
    for tok in it:
        if tok in ("-I", "-L", "-l"):
            arg = next(it, None)
            if arg is None:
                _LOGGER.warning(
                    "Ignoring trailing '%s' in library %s build flags", tok, name
                )
                break
            tok += arg
        if tok.startswith("-I"):
            include_flags.append(tok[2:])
        elif tok.startswith("-L"):
            lib.link_dirs.append((read_path / tok[2:]).resolve())
        elif tok.startswith("-l"):
            lib.link_libs.append(tok[2:])
        elif tok.startswith("-Wl,"):
            lib.link_flags.append(tok)
        else:
            lib.flags.append(tok)

    include_dir = build.get("includeDir", DEFAULT_BUILD_INCLUDE_DIR)
    for d in [include_dir, src_dir, *include_flags]:
        if (path := (read_path / d)).is_dir():
            lib.include_dirs.append(path.resolve())
        elif d in include_flags:
            # The includeDir/srcDir defaults are probes; an explicit -I that
            # does not resolve is a manifest or packaging error worth naming
            _LOGGER.warning(
                "Library %s declares include dir %s which does not exist", name, d
            )

    lib.sources = sorted(
        path.resolve()
        for f in collect_filtered_files(read_path / src_dir, src_filter)
        if (path := Path(f)).suffix in SRC_FILE_EXTENSIONS
    )
    return lib


def _bundled_library(framework_path: Path, name: str) -> ArduinoLibrary:
    """A library bundled with the Arduino core, read from the framework tree."""
    lib_dir = framework_path / "libraries" / name
    manifest = lib_dir / "library.properties"
    data = parse_library_properties(manifest) if manifest.is_file() else {}
    return _library_info(name, lib_dir, {"name": name, **data})


def resolve_libraries(framework_path: Path) -> list[ArduinoLibrary]:
    """Resolve every ``cg.add_library()`` entry into an :class:`ArduinoLibrary`."""
    bundled: list[ArduinoLibrary] = []
    external: list[Library] = []
    for library in CORE.platformio_libraries.values():
        # A version pin means a registry package ("pngle@1.1.0"), never a
        # framework-bundled library.
        if (
            library.repository
            or library.version
            or not library.name
            or "/" in library.name
        ):
            external.append(library)
        elif (framework_path / "libraries" / library.name).is_dir():
            bundled.append(_bundled_library(framework_path, library.name))
        else:
            # A bare registry name; resolved at the latest version, matching
            # PlatformIO (a typo fails loudly as a registry lookup error).
            external.append(library)

    converted: list[ArduinoLibrary] = []
    bundled_names = {lib.name for lib in bundled}

    def _add_bundled_dependencies(component: ConvertedLibrary) -> None:
        # A version-less bare-name dependency ("Hash" in ESPAsyncWebServer)
        # is a core-bundled library; the shared converter skips it because
        # it cannot be resolved from the registry.
        for dep in normalize_dependencies(component.data.get("dependencies")):
            name = dep.get("name")
            if (
                not name
                or dep.get("owner")
                or "version" in dep
                or name in bundled_names
                or not (framework_path / "libraries" / name).is_dir()
            ):
                continue
            try:
                check_library_data(dep, ESP8266_PLATFORM, "arduino")
            except InvalidLibrary as err:
                _LOGGER.debug("Skipping bundled dependency %s: %s", name, err)
                continue
            bundled_names.add(name)
            bundled.append(_bundled_library(framework_path, name))

    def _emit(component: ConvertedLibrary) -> None:
        apply_extra_script(component, "esp8266", pio_platform=ESP8266_PLATFORM)
        converted.append(
            _library_info(
                component.get_require_name(), component.source_dir, component.data
            )
        )
        _add_bundled_dependencies(component)

    if external:
        convert_libraries(
            external,
            LibraryBackend(
                platform=ESP8266_PLATFORM,
                framework="arduino",
                emit=_emit,
                cache_key="arduino8266",
            ),
        )

    return bundled + converted
