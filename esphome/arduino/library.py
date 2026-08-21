"""Arduino-core backend for the shared PlatformIO library converter.

Turns the libraries registered via ``cg.add_library()`` into build inputs for
a native Arduino build. Bare names that exist under the framework's bundled
``libraries/`` directory (ESP8266WiFi, Wire, SPI, ...) are read straight from
the framework tree; everything else goes through the shared
resolution/download pipeline in ``esphome.platformio.library``. Nothing here
is core-specific: the caller names the PlatformIO platform, MCU, and cache
key of the Arduino core it builds.

Known deviations: flat-layout (``library.properties``, no ``src/``)
libraries get the recursive default source filter rather than PlatformIO's
root-only Arduino-1.0 filter (no bundled library is affected), and the
Arduino ``dot_a_linkage`` property is honored even though PlatformIO
ignores it.

Mirrors PlatformIO's ``lib_ldf_mode=off`` behavior: each library builds into
its own static archive and every library's include dir joins one global
include path.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import logging
from pathlib import Path

from esphome.core import CORE, EsphomeError, Library
from esphome.platformio.extra_script import apply_extra_script
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
    is_lib_ignored,
    lex_build_flags,
    lib_ignore_set,
    normalize_dependencies,
    parse_library_json,
    parse_library_properties,
)

_LOGGER = logging.getLogger(__name__)


@dataclass
class ArduinoLibrary:
    """One resolved library, ready for the ninja generator."""

    name: str
    sources: list[Path] = field(default_factory=list)
    include_dirs: list[Path] = field(default_factory=list)
    # Extra compile flags private to this library's own sources
    flags: list[str] = field(default_factory=list)
    # PlatformIO's build.libArchive / Arduino's dot_a_linkage: when False the
    # objects go to the linker directly (symbols nothing references survive)
    lib_archive: bool = True
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
    if "srcDir" in build and not (read_path / src_dir).is_dir():
        # Unlike the default probes, an explicitly declared srcDir that does
        # not resolve is unambiguously a manifest/tree error; a silently
        # empty source set would surface as link errors far from the cause
        raise EsphomeError(
            f"Library {name} declares srcDir {src_dir} which does not exist"
        )

    src_filter = ensure_list(build.get("srcFilter", DEFAULT_BUILD_SRC_FILTER))
    # PlatformIO shell-lexes each build.flags entry
    flag_tokens = lex_build_flags(build.get("flags", []), f"library {name}")

    # build.libArchive is PIO behavior; dot_a_linkage is honored as a
    # deliberate extra (Arduino IDE's property, which PIO ignores) so
    # properties-only libraries can opt out of archiving too
    if "libArchive" in build:
        lib_archive = bool(build["libArchive"])
    elif "dot_a_linkage" in data:
        lib_archive = str(data["dot_a_linkage"]).lower() == "true"
    else:
        lib_archive = True
    lib = ArduinoLibrary(name=name, lib_archive=lib_archive)
    include_flags: list[str] = []
    for tok in flag_tokens:
        if tok.startswith("-I"):
            include_flags.append(tok[2:])
        elif tok.startswith("-L"):
            link_dir = (read_path / tok[2:]).resolve()
            if not link_dir.is_dir():
                # Kept anyway (the linker ignores missing -L dirs); the
                # warning names the culprit before a bare "cannot find -lfoo"
                _LOGGER.warning(
                    "Library %s declares library dir %s which does not exist",
                    name,
                    tok[2:],
                )
            lib.link_dirs.append(link_dir)
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
        elif d in include_flags or (d == include_dir and "includeDir" in build):
            # The includeDir/srcDir defaults are probes; an explicitly
            # declared path that does not resolve is a manifest error
            _LOGGER.warning(
                "Library %s declares include dir %s which does not exist", name, d
            )

    lib.sources = sorted(
        path.resolve()
        for f in collect_filtered_files(read_path / src_dir, src_filter)
        if (path := Path(f)).suffix in SRC_FILE_EXTENSIONS
    )
    if not lib.sources and ("srcFilter" in build or "srcDir" in build):
        # A default probe finding nothing is a header-only library; a
        # declared filter matching nothing is a manifest/tree problem.
        _LOGGER.warning(
            "Library %s declares srcFilter/srcDir but no source files matched",
            name,
        )
    return lib


def _bundled_library(framework_path: Path, name: str) -> ArduinoLibrary:
    """A library bundled with the Arduino core, read from the framework tree.

    ``library.json`` wins over ``library.properties`` when both exist, as in
    PlatformIO's LibBuilderFactory; only the JSON manifest can carry a
    ``build`` section (srcDir, srcFilter, flags).
    """
    lib_dir = framework_path / "libraries" / name
    manifest_json = lib_dir / "library.json"
    if manifest_json.is_file():
        data = parse_library_json(manifest_json)
    else:
        manifest = lib_dir / "library.properties"
        data = parse_library_properties(manifest) if manifest.is_file() else {}
    return _library_info(name, lib_dir, data)


def resolve_libraries(
    framework_path: Path, *, pio_platform: str, board_mcu: str, cache_key: str
) -> list[ArduinoLibrary]:
    """Resolve every ``cg.add_library()`` entry into an :class:`ArduinoLibrary`.

    ``pio_platform``/``board_mcu`` filter manifests the way PlatformIO would
    for that core (e.g. ``espressif8266``/``esp8266``); ``cache_key`` keys the
    shared converter's download cache.
    """
    bundled: list[ArduinoLibrary] = []
    external: list[Library] = []
    # PlatformIO's lib_ignore covers framework-bundled libraries too; the
    # shared converter only filters the registry/git ones.
    lib_ignore = lib_ignore_set()
    for library in CORE.platformio_libraries.values():
        if is_lib_ignored(library.name, lib_ignore):
            continue
        # Only a bare name with a matching framework directory is bundled: a
        # version pin means a registry package ("pngle@1.1.0"), and a bare
        # name without the directory resolves from the registry at the
        # latest version, matching PlatformIO (a typo fails loudly as a
        # registry lookup error).
        if (
            not library.repository
            and not library.version
            and library.name
            and "/" not in library.name
            and (framework_path / "libraries" / library.name).is_dir()
        ):
            # A bundled library's own manifest dependencies are deliberately
            # not walked (PlatformIO's lib_ldf_mode=off does not either);
            # core add_library() calls list what they need explicitly.
            bundled.append(_bundled_library(framework_path, library.name))
        else:
            external.append(library)

    converted: list[ArduinoLibrary] = []
    bundled_names = {lib.name for lib in bundled}

    def _add_bundled_dependencies(component: ConvertedLibrary) -> None:
        # A version-less bare-name dependency ("Hash" in ESPAsyncWebServer)
        # is a core-bundled library; the shared converter skips it because
        # it cannot be resolved from the registry.
        for dep in normalize_dependencies(component.data.get("dependencies")):
            name = dep.get("name")
            if not name:
                _LOGGER.warning(
                    "Ignoring malformed dependency entry %r of library %s",
                    dep,
                    component.name,
                )
                continue
            if name in bundled_names or is_lib_ignored(name, lib_ignore):
                continue
            if "version" in dep:
                # The converter resolves versioned deps from the registry.
                # Note: the dict shorthand {"Wire": "*"} normalizes to
                # version="*" and takes this path even for a bundled name;
                # use the list form for bundled dependencies.
                continue
            if dep.get("owner"):
                # Owner but no version: the converter skips it too, so this
                # is the only place the drop can be made visible
                _LOGGER.warning(
                    "Dependency %s of library %s has an owner but no version "
                    "to resolve; skipping",
                    name,
                    component.name,
                )
                continue
            if not (framework_path / "libraries" / name).is_dir():
                # The shared converter skips version-less deps too, so this
                # is the only place the drop can be made visible before the
                # missing sources surface as link errors.
                _LOGGER.warning(
                    "Dependency %s of library %s is not bundled with the "
                    "framework and has no version to resolve; skipping",
                    name,
                    component.name,
                )
                continue
            try:
                check_library_data(dep, pio_platform, "arduino")
            except InvalidLibrary as err:
                # Rejecting another platform's dependency of a cross-platform
                # manifest is routine (every ESPAsyncWebServer build hits
                # it), so the platform filter stays at debug; any other
                # cause means a dropped dependency and must be visible
                if "platform" in str(err).lower():
                    _LOGGER.debug("Skipping bundled dependency %s: %s", name, err)
                else:
                    _LOGGER.warning(
                        "Skipping bundled dependency %s of %s: %s",
                        name,
                        component.name,
                        err,
                    )
                continue
            bundled_names.add(name)
            bundled.append(_bundled_library(framework_path, name))

    def _emit(component: ConvertedLibrary) -> None:
        apply_extra_script(
            component, board_mcu=lambda: board_mcu, pio_platform=pio_platform
        )
        converted.append(
            _library_info(
                component.get_require_name(), component.source_dir, component.data
            )
        )
        _add_bundled_dependencies(component)

    if external:
        resolved = convert_libraries(
            external,
            LibraryBackend(
                platform=pio_platform,
                framework="arduino",
                emit=_emit,
                cache_key=cache_key,
            ),
        )
        if len(resolved) < len(external):
            # A requested library the converter dropped would otherwise
            # surface only as link errors far from the cause
            _LOGGER.warning(
                "%d of %d requested libraries were not resolved (resolved: %s)",
                len(external) - len(resolved),
                len(external),
                ", ".join(sorted(c.name for c in resolved)) or "none",
            )

    return bundled + converted
