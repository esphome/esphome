"""Arduino-core backend for the shared PlatformIO library converter.

Bundled names build straight from the framework tree; everything else goes
through ``esphome.platformio.library``. Mirrors ``lib_ldf_mode=off``: each
library builds its own archive; all include dirs join one global path.

Deviations from PlatformIO: flat-layout libraries get the recursive default
source filter; ``dot_a_linkage`` is honored; bundled libraries never run a
manifest ``extraScript``; manifest ``-I`` flags join the global include path;
``precompiled``/``ldflags`` properties are not honored (a warning names the
library).
"""

from __future__ import annotations

from dataclasses import dataclass, field
import functools
import logging
from pathlib import Path
import re

from esphome.core import CORE, EsphomeError, Library
from esphome.helpers import walk_files
from esphome.platformio.extra_script import apply_extra_script
from esphome.platformio.library import (
    DEFAULT_BUILD_INCLUDE_DIR,
    DEFAULT_BUILD_SRC_FILTER,
    HEADER_FILE_EXTENSIONS,
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
    warn_properties_depends,
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


# Filename-plain names only: leading alnum/underscore, then word chars,
# dot, space, plus, or hyphen. An allowlist excludes separators, drive
# colons, and dot-only names by shape instead of enumerating them.
_SAFE_LIBRARY_NAME_RE = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_. +-]*\Z")


def _is_safe_library_name(name: object) -> bool:
    """Whether a name may be joined under the framework's libraries dir."""
    return isinstance(name, str) and _SAFE_LIBRARY_NAME_RE.fullmatch(name) is not None


def _manifest_build(name: str, data: object) -> dict:
    """The manifest's ``build`` section; a malformed manifest must fail
    naming the library, not with an AttributeError."""
    build = data.get("build", {}) if isinstance(data, dict) else None
    if not isinstance(build, dict):
        raise EsphomeError(f"Library {name} has a malformed manifest")
    return build


def _resolve_src_dir(name: str, read_path: Path, build: dict) -> str:
    """Resolve PIO's source dir: manifest srcDir, else src/Src, else the root."""
    if "srcDir" not in build:
        return next((d for d in ("src", "Src") if (read_path / d).is_dir()), ".")
    # A declared srcDir (falsy included) that does not resolve is a
    # manifest error
    src_dir = build["srcDir"]
    if not (isinstance(src_dir, str) and src_dir and (read_path / src_dir).is_dir()):
        raise EsphomeError(
            f"Library {name} declares srcDir {src_dir!r} which does not exist"
        )
    return src_dir


def _warn_dropped_link_fields(name: str, data: dict) -> None:
    for dropped_key in ("precompiled", "ldflags"):
        if data.get(dropped_key):
            # PIO's Arduino lib builder honors these; building without them
            # would fail far away at link with no stated cause
            _LOGGER.warning(
                "Library %s declares %s, which this backend does not honor",
                name,
                dropped_key,
            )


def _resolve_lib_archive(name: str, data: dict, build: dict) -> bool:
    """build.libArchive, else dot_a_linkage (Arduino IDE's property, ignored
    by PIO -- a deliberate extra), else archive."""

    # Strict parse: bool("false") is True
    def _parse(key: str, raw: object) -> bool:
        if isinstance(raw, bool):
            return raw
        value = str(raw).strip().lower()
        if value in ("true", "false"):
            return value == "true"
        raise EsphomeError(f"Library {name} has a malformed {key} value {raw!r}")

    if "libArchive" in build:
        return _parse("libArchive", build["libArchive"])
    if "dot_a_linkage" in data:
        return _parse("dot_a_linkage", data["dot_a_linkage"])
    return True


def _classify_build_flags(
    name: str, read_path: Path, lib: ArduinoLibrary, flag_tokens: list[str]
) -> list[str]:
    """Route the lexed build.flags into the library's flag lists.

    Returns the ``-I`` arguments for the include-dir resolution.
    """
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
    return include_flags


def _resolve_include_dirs(
    name: str,
    read_path: Path,
    lib: ArduinoLibrary,
    build: dict,
    src_dir: str,
    include_flags: list[str],
) -> None:
    include_dir = build.get("includeDir", DEFAULT_BUILD_INCLUDE_DIR)
    if not isinstance(include_dir, str):
        raise EsphomeError(f"Library {name} has a malformed includeDir")
    for d, explicit in [
        (include_dir, "includeDir" in build),
        (src_dir, False),  # _resolve_src_dir already validated it
        *((flag, True) for flag in include_flags),
    ]:
        if (path := (read_path / d)).is_dir():
            lib.include_dirs.append(path.resolve())
        elif explicit:
            # Warn-and-drop is intended (unlike srcDir, which raises): a
            # missing include dir is harmless until a header is actually
            # needed, and the compile names it then
            _LOGGER.warning(
                "Library %s declares include dir %s which does not exist", name, d
            )


def _collect_lib_sources(
    name: str,
    read_path: Path,
    lib: ArduinoLibrary,
    build: dict,
    src_dir: str,
    src_filter: list[str],
) -> None:
    matched = collect_filtered_files(read_path / src_dir, src_filter)
    lib.sources = sorted(
        path.resolve()
        for f in matched
        if (path := Path(f)).suffix in SRC_FILE_EXTENSIONS
    )
    # A source-like suffix the case-sensitive map rejects (.CPP, .ino) is a
    # dropped compilation unit that surfaces as undefined symbols at link;
    # headers and metadata files fall through silently (header-only
    # libraries are routine)
    source_like = {s.lower() for s in SRC_FILE_EXTENSIONS} | {".ino"}
    if dropped := [
        Path(f).name
        for f in matched
        if Path(f).suffix not in SRC_FILE_EXTENSIONS
        and Path(f).suffix.lower() in source_like
    ]:
        _LOGGER.warning(
            "Library %s: %d file(s) with unmapped source suffixes are not compiled: %s",
            name,
            len(dropped),
            ", ".join(sorted(dropped)),
        )
    if not lib.sources and not matched and ("srcFilter" in build or "srcDir" in build):
        # A default probe finding nothing is a header-only library; a
        # declared filter matching nothing is a manifest/tree problem.
        _LOGGER.warning(
            "Library %s declares srcFilter/srcDir but no source files matched",
            name,
        )


def _library_info(name: str, read_path: Path, data: dict) -> ArduinoLibrary:
    """Resolve one library's sources, include dirs, and flags (PIO semantics)."""
    build = _manifest_build(name, data)
    _warn_dropped_link_fields(name, data)
    src_dir = _resolve_src_dir(name, read_path, build)
    src_filter = ensure_list(build.get("srcFilter", DEFAULT_BUILD_SRC_FILTER))
    if not all(isinstance(entry, str) for entry in src_filter):
        raise EsphomeError(f"Library {name} has a malformed srcFilter")
    lib = ArduinoLibrary(name=name, lib_archive=_resolve_lib_archive(name, data, build))
    # PlatformIO shell-lexes each build.flags entry
    include_flags = _classify_build_flags(
        name, read_path, lib, lex_build_flags(build.get("flags", []), f"library {name}")
    )
    _resolve_include_dirs(name, read_path, lib, build, src_dir, include_flags)
    _collect_lib_sources(name, read_path, lib, build, src_dir, src_filter)
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
        # Bundled manifest deps are never walked; make the skip visible
        if data.get("dependencies"):
            _LOGGER.warning(
                "Bundled library %s declares dependencies, which are not "
                "resolved automatically; add them with add_library() if needed",
                name,
            )
        warn_properties_depends(name, data)
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
        Path(p).suffix.lower() in HEADER_FILE_EXTENSIONS for p in walk_files(lib_dir)
    ):
        # An empty or half-extracted bundled directory can never link; a
        # warning would scroll away and resurface as undefined symbols
        raise EsphomeError(
            f"Bundled library {name} has no sources or headers; the "
            "framework install may be incomplete (run 'esphome clean-all')"
        )
    return lib


def _external_short_name(name: str) -> str:
    """The short library name of a requested spec.

    "owner/Name" and plain names take the last path segment; the
    "Name=<url>" custom-name form takes the declared name (the URL tail is
    a repository path, not a library name).
    """
    head, sep, tail = name.partition("=")
    if sep and "://" in tail:
        return head
    return name.rsplit("/", maxsplit=1)[-1]


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
    # Memoized "does the framework bundle this name?"; the safety guard and
    # dir probe must stay fused (path traversal)
    _provided = functools.cache(
        lambda name: (
            _is_safe_library_name(name)
            and (framework_path / "libraries" / name).is_dir()
        )
    )
    for library in CORE.platformio_libraries.values():
        if is_lib_ignored(library.name, lib_ignore):
            continue
        # Bundled only for a bare name with a matching framework dir; pinned
        # or unmatched names resolve from the registry, as under PlatformIO.
        if not library.repository and not library.version and _provided(library.name):
            # Bundled libraries' own manifest deps are not walked (none of
            # the ESP8266 core's declare any; _bundled_library warns if one does)
            bundled.append(_bundled_library(framework_path, library.name))
        else:
            external.append(library)

    converted: list[ArduinoLibrary] = []
    bundled_names = {lib.name for lib in bundled}
    converted_manifest_names: set[str] = set()
    # Ordered set of bundled dependency names to add once conversion is done
    pending_bundled: dict[str, None] = {}
    # Deps matching a separately-requested external are already in the build
    # (a duplicate archive means duplicate-symbol link errors)
    external_short_names = {
        _external_short_name(lib.name) for lib in external if lib.name
    }

    def _add_bundled_dependencies(component: ConvertedLibrary) -> None:
        # A version-less bare-name dependency ("Hash" in ESPAsyncWebServer)
        # is a core-bundled library; the shared converter skips it because
        # it cannot be resolved from the registry.
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
            if dep.get("owner") or not _provided(name):
                # Owner-less names in the framework tree prefer the bundled
                # copy (PIO's process_dependencies); everything else resolves
                # via the converter, and the walk reports any real drops
                continue
            if not dependency_is_usable(dep, pio_platform, "arduino", component.name):
                continue
            # Deferred: a later-emitted library's manifest name may satisfy
            # this; adding now could double the archive
            pending_bundled.setdefault(name)

    def _emit(component: ConvertedLibrary) -> None:
        apply_extra_script(
            component, board_mcu=lambda: board_mcu, pio_platform=pio_platform
        )
        if isinstance(manifest_name := component.data.get("name"), str):
            converted_manifest_names.add(manifest_name)
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
                platform=pio_platform,
                framework="arduino",
                emit=_emit,
                cache_key=cache_key,
                # The walk must not resolve bundled names from the registry;
                # _add_bundled_dependencies adds them after emit
                provides=_provided,
            ),
        )
    for name in pending_bundled:
        if name in converted_manifest_names or name in bundled_names:
            continue
        bundled_names.add(name)
        bundled.append(_bundled_library(framework_path, name))

    return bundled + converted
