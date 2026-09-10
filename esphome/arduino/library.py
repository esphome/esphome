"""Arduino-core backend for the shared PlatformIO library converter.

Bundled names build straight from the framework tree; everything else goes
through ``esphome.platformio.library``. Mirrors ``lib_ldf_mode=off``: each
library builds its own archive; all include dirs join one global path.

Deviations from PlatformIO: flat-layout libraries get the recursive default
source filter; ``dot_a_linkage`` is honored; bundled libraries never run a
manifest ``extraScript``; manifest ``-I`` flags join the global include path;
``precompiled``/``ldflags`` properties are refused by name.
"""

from __future__ import annotations

from dataclasses import dataclass, field
import logging
from pathlib import Path
import re

from esphome.core import CORE, EsphomeError, Library
from esphome.helpers import walk_files
from esphome.platformio.extra_script import apply_extra_script
from esphome.platformio.library import (
    DEFAULT_BUILD_INCLUDE_DIR,
    DEFAULT_BUILD_SRC_FILTER,
    ESPHOME_DATA_KEY,
    ESPHOME_DATA_LINK_FLAGS_KEY,
    LIBRARY_HEADER_SUFFIXES,
    SRC_FILE_EXTENSIONS,
    ConvertedLibrary,
    IncompatiblePlatform,
    InvalidLibrary,
    LibraryBackend,
    _url_or_none,
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


# Source-like suffixes the case-sensitive suffix map rejects
_UNMAPPED_SOURCE_SUFFIXES = frozenset(
    {s.lower() for s in SRC_FILE_EXTENSIONS} | {".ino"}
)

# Filename-plain names: an allowlist excludes separators, drive colons,
# and dot-only names by shape
_SAFE_LIBRARY_NAME_RE = re.compile(r"[A-Za-z0-9_][A-Za-z0-9_. +-]*\Z")


def _is_safe_library_name(name: object) -> bool:
    """Whether a name may be joined under the framework's libraries dir."""
    return isinstance(name, str) and _SAFE_LIBRARY_NAME_RE.fullmatch(name) is not None


def _manifest_build(name: str, data: object) -> dict:
    """The manifest's ``build`` section; malformed manifests fail by name."""
    build = data.get("build", {}) if isinstance(data, dict) else None
    if not isinstance(build, dict):
        raise EsphomeError(f"Library {name} has a malformed manifest")
    return build


def _resolve_src_dir(name: str, read_path: Path, build: dict) -> str:
    """Resolve PIO's source dir: manifest srcDir, else src/Src, else the root."""
    if "srcDir" not in build:
        return next((d for d in ("src", "Src") if (read_path / d).is_dir()), ".")
    # A declared srcDir (falsy included) that does not resolve is a manifest error
    src_dir = build["srcDir"]
    if not (isinstance(src_dir, str) and src_dir and (read_path / src_dir).is_dir()):
        raise EsphomeError(
            f"Library {name} declares srcDir {src_dir!r} which does not exist"
        )
    return src_dir


def _reject_unsupported_link_fields(name: str, data: dict) -> None:
    # PIO honors these; ignoring them would fail at link with no stated
    # cause. Property values are strings, so "false" is not a declaration.
    precompiled = data.get("precompiled")
    if precompiled and str(precompiled).strip().lower() != "false":
        raise EsphomeError(
            f"Library {name} declares precompiled, which this backend does not support"
        )
    if data.get("ldflags"):
        raise EsphomeError(
            f"Library {name} declares ldflags, which this backend does not support"
        )


def _resolve_lib_archive(name: str, data: dict, build: dict) -> bool:
    """build.libArchive, else dot_a_linkage (an Arduino IDE property PIO
    ignores; a deliberate extra), else archive."""

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
                # Kept (the linker ignores missing -L dirs); the warning
                # names the culprit before a bare "cannot find -lfoo"
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
            # Warn-and-drop (unlike srcDir): a missing include dir is
            # harmless until a header is needed, and the compile names it
            _LOGGER.warning(
                "Library %s declares include dir %s which does not exist", name, d
            )


def _collect_lib_sources(
    name: str,
    read_path: Path,
    lib: ArduinoLibrary,
    src_dir: str,
    src_filter: list[str],
) -> None:
    sources: list[Path] = []
    dropped: list[str] = []
    saw_header = False
    for f in collect_filtered_files(read_path / src_dir, src_filter):
        path = Path(f)
        suffix = path.suffix
        if suffix in SRC_FILE_EXTENSIONS:
            # resolve() per file: srcFilter patterns may escape src_dir
            sources.append(path.resolve())
        elif suffix.lower() in _UNMAPPED_SOURCE_SUFFIXES:
            # A source-like suffix the case-sensitive map rejects (.CPP,
            # .ino) is a dropped compilation unit; headers fall through
            dropped.append(path.name)
        elif suffix.lower() in LIBRARY_HEADER_SUFFIXES:
            saw_header = True
    lib.sources = sorted(sources)
    if dropped:
        _LOGGER.warning(
            "Library %s: %d file(s) with unmapped source suffixes are not compiled: %s",
            name,
            len(dropped),
            ", ".join(sorted(dropped)),
        )
    if not lib.sources and not saw_header:
        # Matched headers mean header-only; a filter matching nothing is
        # a manifest/tree problem (a truly empty tree raises elsewhere)
        _LOGGER.warning("Library %s: no source files matched", name)


def _library_info(name: str, read_path: Path, data: dict) -> ArduinoLibrary:
    """Resolve one library's sources, include dirs, and flags (PIO semantics)."""
    build = _manifest_build(name, data)
    _reject_unsupported_link_fields(name, data)
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
    _collect_lib_sources(name, read_path, lib, src_dir, src_filter)
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
        try:
            data = parse_library_json(manifest_json)
        except ValueError as err:  # JSONDecodeError
            raise EsphomeError(
                f"Bundled library {name} has a corrupt library.json ({err}); "
                "the framework install may be incomplete (run 'esphome clean-all')"
            ) from err
    elif (manifest := lib_dir / "library.properties").is_file():
        data = parse_library_properties(manifest)
    else:
        # Debug, not warning: the legacy manifest-less layout is legal and
        # the 3.1.2 core ships one such library (FSTools), so a warning
        # would be unactionable noise on every build using it
        _LOGGER.debug("Bundled library %s has no manifest; using defaults", name)
        data = {}
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
            # Scripts only run on the converted path; building without
            # the script's flags would miscompile
            raise EsphomeError(
                f"Bundled library {name} declares an extraScript, which is "
                "not run for bundled libraries"
            )
    lib = _library_info(name, lib_dir, data)
    _assert_tree_has_code(
        name,
        lib_dir,
        "the framework install may be incomplete (run 'esphome clean-all')",
    )
    return lib


def _assert_tree_has_code(name: str, root: Path, hint: str) -> None:
    """An empty or half-extracted tree can never link; fail by name (a
    warning would scroll away and resurface as undefined symbols)."""
    if not any(
        Path(p).suffix in SRC_FILE_EXTENSIONS
        or Path(p).suffix.lower() in LIBRARY_HEADER_SUFFIXES
        for p in walk_files(root)
    ):
        raise EsphomeError(f"Library {name} has no sources or headers; {hint}")


def _external_short_name(name: str) -> str:
    """The short library name of a requested spec.

    "owner/Name" and plain names take the last path segment; "Name=<url>"
    takes the declared name. Git tails (".git", "#ref") are stripped like
    the walk's URL normalization; the comparand is a manifest dependency
    name, never a spec.
    """
    head, sep, tail = name.partition("=")
    if sep and "://" in tail:
        return head
    short = name.rsplit("/", maxsplit=1)[-1]
    return short.partition("#")[0].removesuffix(".git")


def _check_unfulfilled_provides(
    provided_requests: set[str], satisfied: set[str], still_requested: set[str]
) -> None:
    """Fail by name when a walk-skipped dependency was never added.

    An unfulfilled provides() promise only surfaces as undefined symbols
    at link. The walk records across re-resolutions, so a name no final
    manifest still requests is stale state, never a failure.
    """
    if missing := sorted((provided_requests & still_requested) - satisfied):
        raise EsphomeError(
            "provides() skipped these dependencies but nothing added them: "
            f"{', '.join(missing)}; the build is missing libraries"
        )


def resolve_libraries(
    framework_path: Path, *, pio_platform: str, board_mcu: str, cache_key: str
) -> list[ArduinoLibrary]:
    """Resolve every ``cg.add_library()`` entry into an :class:`ArduinoLibrary`.

    ``pio_platform``/``board_mcu`` filter manifests the way PlatformIO would
    for that core (e.g. ``espressif8266``/``esp8266``); ``cache_key`` keys the
    shared converter's download cache.

    The returned list is not topologically sorted, so the caller must link
    the archives inside one ``--start-group``/``--end-group`` pair (the
    bundled-first grouping is incidental).
    """
    bundled: list[ArduinoLibrary] = []
    external: list[Library] = []
    # PlatformIO's lib_ignore covers framework-bundled libraries too; the
    # shared converter only filters the registry/git ones.
    lib_ignore = lib_ignore_set()
    # Exact directory names keep membership case-sensitive everywhere
    # (an is_dir() probe would match "wire" on macOS/Windows and build
    # the bundled Wire twice)
    libraries_dir = framework_path / "libraries"
    if not libraries_dir.is_dir():
        # A registry fallback would fail later with a misleading
        # package-not-found error per bundled name
        raise EsphomeError(
            f"{libraries_dir} is missing; the framework install may be "
            "incomplete (run 'esphome clean-all')"
        )
    bundled_dir_names = frozenset(p.name for p in libraries_dir.iterdir() if p.is_dir())

    def _provided(name: object) -> bool:
        return _is_safe_library_name(name) and name in bundled_dir_names

    for library in CORE.platformio_libraries.values():
        if is_lib_ignored(library.name, lib_ignore):
            continue
        # Bundled only for a bare name with a matching framework dir; pinned
        # or unmatched names resolve from the registry, as under PlatformIO.
        if not library.repository and not library.version and _provided(library.name):
            # Bundled manifest deps are not walked; _bundled_library warns
            bundled.append(_bundled_library(framework_path, library.name))
        else:
            external.append(library)

    converted: list[ArduinoLibrary] = []
    bundled_names = {lib.name for lib in bundled}
    converted_manifest_names: set[str] = set()
    # Bundled candidates skipped on purpose (platform filter); the
    # provides() reconciliation must count them as satisfied
    knowingly_skipped: set[str] = set()
    # Dependency names of the manifests actually emitted; a walk recording
    # for a since-re-resolved manifest must not fail the reconciliation
    final_dep_names: set[str] = set()
    # Ordered set of bundled dependency names to add once conversion is done
    pending_bundled: dict[str, None] = {}
    # Deps matching a separately-requested external are already in the build
    # (a duplicate archive means duplicate-symbol link errors)
    external_short_names = {
        _external_short_name(lib.name) for lib in external if lib.name
    }

    def _add_bundled_dependencies(component: ConvertedLibrary) -> None:
        # A version-less bare name ("Hash") is a core-bundled library the
        # shared converter cannot resolve from the registry
        for dep in normalize_dependencies(
            component.data.get("dependencies"), component.name
        ):
            # normalize_dependencies guarantees a non-empty str name
            name = dep["name"]
            final_dep_names.add(name)
            if "/" in name:
                owner, _, pkg = name.partition("/")
                if _is_safe_library_name(owner) and _is_safe_library_name(pkg):
                    # Owner-qualified; the converter resolves it from the registry
                    continue
            if not _is_safe_library_name(name):
                # The name becomes a path component; never join a traversal
                _LOGGER.warning(
                    "Ignoring malformed dependency entry %r of library %s",
                    dep,
                    component.name,
                )
                continue
            if name in external_short_names:
                if _provided(name):
                    # A bundled copy is suppressed; a coincidental name
                    # collision would surface as link errors
                    _LOGGER.warning(
                        "Dependency %s of %s is assumed satisfied by a "
                        "requested external library; the bundled copy is "
                        "not added",
                        name,
                        component.name,
                    )
                else:
                    _LOGGER.debug(
                        "Dependency %s of %s assumed satisfied by a requested "
                        "external library",
                        name,
                        component.name,
                    )
                continue
            if name in bundled_names or is_lib_ignored(name, lib_ignore):
                continue
            if _url_or_none(dep.get("version")) is not None:
                # A URL names one specific source; never add the bundled copy
                continue
            if dep.get("owner") or not _provided(name):
                # Only owner-less framework-tree names take the bundled
                # copy (PIO's process_dependencies); the walk reports drops
                continue
            try:
                # framework=None: the walk already warned for non-platform
                # causes; debug keeps one fault from warning twice (pinned
                # by test_nonplatform_rejection_warns_once_through_real_converter)
                check_library_data(dep, pio_platform, None)
            except IncompatiblePlatform as err:
                # A knowing skip (platform filter), not a broken promise
                knowingly_skipped.add(name)
                _LOGGER.debug("Skip bundled candidate %s: %s", name, err)
                continue
            except InvalidLibrary as err:
                # Malformed manifest data never counts as satisfied; the
                # walk owns the warning (see the warns-once test above)
                _LOGGER.debug("Skip malformed bundled candidate %s: %s", name, err)
                continue
            # Deferred: a later manifest name may satisfy this
            pending_bundled.setdefault(name)

    def _emit(component: ConvertedLibrary) -> None:
        apply_extra_script(
            component, board_mcu=lambda: board_mcu, pio_platform=pio_platform
        )
        _assert_tree_has_code(
            component.get_require_name(),
            component.source_dir,
            "the download may be incomplete (run 'esphome clean-all')",
        )
        if isinstance(manifest_name := component.data.get("name"), str):
            converted_manifest_names.add(manifest_name)
        lib = _library_info(
            component.get_require_name(), component.source_dir, component.data
        )
        # Extra-script LINKFLAGS travel outside build.flags; dropping
        # them would link wrong with no stated cause
        lib.link_flags.extend(
            component.data.get(ESPHOME_DATA_KEY, {}).get(
                ESPHOME_DATA_LINK_FLAGS_KEY, []
            )
        )
        converted.append(lib)
        _add_bundled_dependencies(component)

    backend = LibraryBackend(
        platform=pio_platform,
        framework="arduino",
        emit=_emit,
        cache_key=cache_key,
        # The walk must not resolve bundled names from the registry;
        # _add_bundled_dependencies adds them after emit
        provides=_provided,
    )
    if external:
        convert_libraries(external, backend)
    for name in pending_bundled:
        if name in converted_manifest_names:
            # The converted library is this one; the bundled copy would
            # double the archive. Warn like the external_short_names twin.
            _LOGGER.warning(
                "Dependency %s is assumed satisfied by a converted library's "
                "manifest name; the bundled copy is not added",
                name,
            )
            continue
        bundled_names.add(name)
        bundled.append(_bundled_library(framework_path, name))

    _check_unfulfilled_provides(
        backend.provided_requests,
        bundled_names
        | converted_manifest_names
        | external_short_names
        | knowingly_skipped,
        final_dep_names,
    )

    return bundled + converted
