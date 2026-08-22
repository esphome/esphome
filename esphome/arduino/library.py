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
ignores it. Bundled libraries never run a manifest ``extraScript`` (a
warning names the library if one declares it). Manifest ``-I`` build
flags join the global include path rather than staying private to the
library's own sources as under PlatformIO.

Mirrors PlatformIO's ``lib_ldf_mode=off`` behavior: each library builds into
its own static archive and every library's include dir joins one global
include path.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import functools
import logging
from pathlib import Path

from esphome.core import CORE, EsphomeError, Library
from esphome.helpers import walk_files
from esphome.platformio.extra_script import apply_extra_script
from esphome.platformio.library import (
    DEFAULT_BUILD_INCLUDE_DIR,
    DEFAULT_BUILD_SRC_FILTER,
    SRC_FILE_EXTENSIONS,
    ConvertedLibrary,
    LibraryBackend,
    collect_filtered_files,
    convert_libraries,
    dependency_is_usable,
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


def _is_safe_library_name(name: object) -> bool:
    """Whether a name may be joined under the framework's libraries dir."""
    return (
        isinstance(name, str)
        and bool(name)
        and "/" not in name
        and "\\" not in name
        and ":" not in name  # a Windows drive-relative name escapes the tree
        and name not in (".", "..")
    )


def _warn_properties_depends(name: str, data: object) -> None:
    """Warn when a manifest declares dependencies only as ``depends=``.

    The dependency walk reads the JSON ``dependencies`` key; the raw
    ``library.properties`` spelling would otherwise drop silently.
    """
    if isinstance(data, dict) and not data.get("dependencies") and data.get("depends"):
        _LOGGER.warning(
            "Library %s declares dependencies via library.properties "
            "depends=, which are not resolved automatically; add them with "
            "add_library() if needed",
            name,
        )


def _manifest_build(name: str, data: object) -> dict:
    """The manifest's ``build`` section, validated by name.

    A bare json.load imposes no shape; a malformed manifest must name the
    library instead of an AttributeError deep in a traceback (and must do so
    before apply_extra_script dereferences the same section).
    """
    build = data.get("build", {}) if isinstance(data, dict) else None
    if not isinstance(build, dict):
        raise EsphomeError(f"Library {name} has a malformed manifest")
    return build


def _library_info(name: str, read_path: Path, data: dict) -> ArduinoLibrary:
    """Resolve one library's sources, include dirs, and flags (PIO semantics)."""
    build = _manifest_build(name, data)

    # PIO's source-dir resolution: manifest srcDir, else src/Src, else the root
    if "srcDir" in build:
        # An explicitly declared srcDir (falsy included) that does not
        # resolve is unambiguously a manifest/tree error; a silently empty
        # source set would surface as link errors far from the cause
        src_dir = build["srcDir"]
        if not (
            isinstance(src_dir, str) and src_dir and (read_path / src_dir).is_dir()
        ):
            raise EsphomeError(
                f"Library {name} declares srcDir {src_dir!r} which does not exist"
            )
    else:
        src_dir = next((d for d in ("src", "Src") if (read_path / d).is_dir()), ".")

    src_filter = ensure_list(build.get("srcFilter", DEFAULT_BUILD_SRC_FILTER))
    if not all(isinstance(entry, str) for entry in src_filter):
        raise EsphomeError(f"Library {name} has a malformed srcFilter")
    # PlatformIO shell-lexes each build.flags entry
    flag_tokens = lex_build_flags(build.get("flags", []), f"library {name}")

    # build.libArchive is PIO behavior; dot_a_linkage is honored as a
    # deliberate extra (Arduino IDE's property, which PIO ignores) so
    # properties-only libraries can opt out of archiving too. Both parse
    # through the same strict table: bool("false") is True, and a typo'd
    # value must not silently change link semantics.
    def _parse_archive(key: str, raw: object) -> bool:
        if isinstance(raw, bool):
            return raw
        value = str(raw).strip().lower()
        if value in ("true", "false"):
            return value == "true"
        _LOGGER.warning(
            "Library %s has an unrecognized %s value %r; assuming true",
            name,
            key,
            raw,
        )
        return True

    if "libArchive" in build:
        lib_archive = _parse_archive("libArchive", build["libArchive"])
    elif "dot_a_linkage" in data:
        lib_archive = _parse_archive("dot_a_linkage", data["dot_a_linkage"])
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
    if not isinstance(include_dir, str):
        raise EsphomeError(f"Library {name} has a malformed includeDir")
    for d, explicit in [
        (include_dir, "includeDir" in build),
        (src_dir, False),  # the srcDir guard above already validated it
        *((flag, True) for flag in include_flags),
    ]:
        if (path := (read_path / d)).is_dir():
            lib.include_dirs.append(path.resolve())
        elif explicit:
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
    if isinstance(data, dict):
        # The dependency walk never runs for bundled libraries (a no-op for
        # the ESP8266 core, whose bundled manifests declare none); on a core
        # where one does, the skip must be visible before link errors
        if data.get("dependencies"):
            _LOGGER.warning(
                "Bundled library %s declares dependencies, which are not "
                "resolved automatically; add them with add_library() if needed",
                name,
            )
        _warn_properties_depends(name, data)
        build = data.get("build")
        if isinstance(build, dict) and build.get("extraScript"):
            # apply_extra_script only runs on the converted path; a bundled
            # manifest relying on one would build with missing flags
            _LOGGER.warning(
                "Bundled library %s declares an extraScript, which is not "
                "run for bundled libraries",
                name,
            )
    lib = _library_info(name, lib_dir, data)
    if not lib.sources and not any(
        Path(p).suffix in (".h", ".hpp", ".hh", ".inc")
        for d in lib.include_dirs
        for p in walk_files(d)
    ):
        # An empty or half-extracted bundled directory can never link; a
        # warning would scroll away and resurface as undefined symbols
        raise EsphomeError(
            f"Bundled library {name} has no sources or headers; the "
            "framework install may be incomplete (run 'esphome clean-all')"
        )
    return lib


def resolve_libraries(
    framework_path: Path, *, pio_platform: str, board_mcu: str, cache_key: str
) -> list[ArduinoLibrary]:
    """Resolve every ``cg.add_library()`` entry into an :class:`ArduinoLibrary`.

    ``pio_platform``/``board_mcu`` filter manifests the way PlatformIO would
    for that core (e.g. ``espressif8266``/``esp8266``); ``cache_key`` keys the
    shared converter's download cache.

    The returned order is unordered with respect to link dependencies
    (bundled dependencies precede their dependents); the caller must link
    the archives inside one ``--start-group``/``--end-group`` pair.
    """
    bundled: list[ArduinoLibrary] = []
    external: list[Library] = []
    # PlatformIO's lib_ignore covers framework-bundled libraries too; the
    # shared converter only filters the registry/git ones.
    lib_ignore = lib_ignore_set()
    # One memoized answer to "does the framework bundle this name?" for the
    # classification loop, the provides hook, and the dependency walk: the
    # safety guard and the dir probe must stay fused (path traversal), and
    # common names (Wire, SPI) are asked repeatedly
    _provided = functools.cache(
        lambda name: (
            _is_safe_library_name(name)
            and (framework_path / "libraries" / name).is_dir()
        )
    )
    for library in CORE.platformio_libraries.values():
        if is_lib_ignored(library.name, lib_ignore):
            continue
        # Only a bare name with a matching framework directory is bundled: a
        # version pin means a registry package ("pngle@1.1.0"), and a bare
        # name without the directory resolves from the registry at the
        # latest version, matching PlatformIO (a typo fails loudly as a
        # registry lookup error).
        if not library.repository and not library.version and _provided(library.name):
            # A bundled library's own manifest dependencies are not walked.
            # PlatformIO would walk them even under lib_ldf_mode=off, but no
            # library bundled with the ESP8266 core declares any, so the walk
            # is a no-op there; core add_library() calls list what they need.
            bundled.append(_bundled_library(framework_path, library.name))
        else:
            external.append(library)

    converted: list[ArduinoLibrary] = []
    bundled_names = {lib.name for lib in bundled}
    # Short names of the separately-requested externals: a manifest
    # dependency matching one is already in the build, not a drop (a false
    # "skipping" warning teaches users to ignore the real one)
    external_short_names = {lib.name.split("/")[-1] for lib in external if lib.name}

    def _add_bundled_dependencies(component: ConvertedLibrary) -> None:
        # A version-less bare-name dependency ("Hash" in ESPAsyncWebServer)
        # is a core-bundled library; the shared converter skips it because
        # it cannot be resolved from the registry.
        _warn_properties_depends(component.name, component.data)
        for dep in normalize_dependencies(
            component.data.get("dependencies"), component.name
        ):
            name = dep.get("name")
            if not _is_safe_library_name(name):
                # The name becomes a path component under the framework
                # tree; never join a traversal or a non-string
                _LOGGER.warning(
                    "Ignoring malformed dependency entry %r of library %s",
                    dep,
                    component.name,
                )
                continue
            if (
                name in bundled_names
                or name in external_short_names
                or is_lib_ignored(name, lib_ignore)
            ):
                continue
            if "version" in dep and (dep.get("owner") or not _provided(name)):
                # The converter resolves versioned deps from the registry. An
                # owner-less versioned name that exists in the framework tree
                # ({"Wire": "*"} normalizes to version="*") falls through to
                # the bundled path below, matching PlatformIO's
                # process_dependencies preference for bundled builders.
                continue
            if dep.get("owner") or not _provided(name):
                # The shared walk's post-emit reconciliation reports drops
                # (it alone knows the final resolution set); nothing to add
                continue
            if not dependency_is_usable(dep, pio_platform, "arduino", component.name):
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
        # Every converter drop path raises (an incompatible top-level is a
        # RuntimeError, resolution and download failures raise), so the
        # return needs no re-verification here.
        convert_libraries(
            external,
            LibraryBackend(
                platform=pio_platform,
                framework="arduino",
                emit=_emit,
                cache_key=cache_key,
                # The graph walk must not resolve a bundled name from the
                # registry ({"Wire": "*"} in a manifest); the bundled copy
                # is added by _add_bundled_dependencies after emit. Unsafe
                # names are simply not provided.
                provides=_provided,
            ),
        )

    return bundled + converted
