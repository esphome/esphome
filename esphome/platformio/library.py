"""Toolchain-agnostic PlatformIO library converter.

Resolves a batch of PlatformIO/Arduino library specs (added via
``cg.add_library(...)``) into local, build-ready directories: it fetches each
library (registry/git/url), parses its ``library.json`` / ``library.properties``
manifest, resolves the whole dependency graph to a single version per name, and
caches the result under ``<data_dir>/pio_components``.

The toolchain-specific part — turning a resolved library into build files
(ESP-IDF ``idf_component_register`` CMakeLists, or a Zephyr module) — is supplied
by a :class:`LibraryBackend`. This module owns everything that is the same
regardless of which toolchain consumes the result.
"""

from collections import deque
from collections.abc import Callable, Iterable
from dataclasses import dataclass, field
from functools import partial
import glob
import hashlib
import itertools
import json
import logging
import os
from pathlib import Path, PurePosixPath
import re
from typing import Any
from urllib.parse import urlsplit, urlunsplit
from urllib.request import url2pathname

from esphome import git
from esphome.core import CORE, EsphomeError, Library
from esphome.framework_helpers import (
    download_and_extract,
    failure_reason,
    rmdir,
    run_batch_downloads,
    warn_prefetch_failures,
)

_LOGGER = logging.getLogger(__name__)

PathType = str | os.PathLike

#
# Constants from platformio
#

FILTER_REGEX = re.compile(r"([+-])<([^>]+)>")
DEFAULT_BUILD_SRC_FILTER = (
    "+<*> -<.git/> -<.svn/> -<example/> -<examples/> -<test/> -<tests/>"
)
DEFAULT_BUILD_SRC_DIRS = "src"
DEFAULT_BUILD_INCLUDE_DIR = "include"
DEFAULT_BUILD_FLAGS = []
# Suffix -> compiler kind (PlatformIO's CSUFFIXES/CXXSUFFIXES/ASSUFFIXES);
# "aspp" is SCons's preprocessed-assembly set, "asm" its plain-assembler
# set (no preprocessor, defines, or includes). Per CXXSUFFIXES .C/.C++ are
# C++ here, even where SCons demotes .C on case-insensitive filesystems.
SOURCE_KIND_FOR_SUFFIX: dict[str, str] = {
    ".c": "c",
    ".cpp": "cxx",
    ".cc": "cxx",
    ".cxx": "cxx",
    ".c++": "cxx",
    ".C": "cxx",
    ".C++": "cxx",
    ".S": "aspp",
    ".spp": "aspp",
    ".SPP": "aspp",
    ".sx": "aspp",
    ".s": "asm",
    ".asm": "asm",
    ".ASM": "asm",
}
SRC_FILE_EXTENSIONS = list(SOURCE_KIND_FOR_SUFFIX)

DOMAIN = "pio_components"

# Marks a cache dir whose archive finished extracting; a missing marker
# means a torn extraction that must be redone
_EXTRACTED_MARKER = ".esphome_extracted"

ESPHOME_DATA_KEY = "ESPHOME"
ESPHOME_DATA_EXTRA_CMAKE_KEY = "EXTRA_CMAKE"
# Captured extra-script LINKFLAGS; kept apart from build.flags so they reach
# the link line (target_link_options), not target_compile_options
ESPHOME_DATA_LINK_FLAGS_KEY = "LINK_FLAGS"


class Source:
    def download(
        self, dir_suffix: str, force: bool = False, salt: str = "", namespace: str = ""
    ) -> Path:
        raise NotImplementedError

    def source_root(self, build_path: Path) -> Path:
        """Directory holding the library's own files (manifest + sources).

        Defaults to the downloaded build directory; a source that references its
        files in place (:class:`LocalSource`) overrides this to point elsewhere.
        """
        return build_path


class URLSource(Source):
    def __init__(self, url: str, size: int | None = None):
        self.url = url
        # Archive size as reported by the registry, when known; sizes the
        # combined prefetch bar without any extra network probe
        self.size = size

    def _cache_dir(self, dir_suffix: str, salt: str, namespace: str) -> Path:
        # Namespace the cache per backend (e.g. pio_components/idf, .../zephyr) so
        # the build files each backend writes into the library dir can't collide.
        base_dir = Path(CORE.data_dir) / DOMAIN
        if namespace:
            base_dir = base_dir / namespace
        h = hashlib.new("sha256")
        h.update(self.url.encode())
        if salt:
            h.update(salt.encode())
        return base_dir / h.hexdigest()[:8] / dir_suffix

    def is_cached(self, dir_suffix: str, salt: str = "", namespace: str = "") -> bool:
        """Whether a completed extraction already exists for this source."""
        return (
            self._cache_dir(dir_suffix, salt, namespace) / _EXTRACTED_MARKER
        ).is_file()

    def download(
        self,
        dir_suffix: str,
        force: bool = False,
        salt: str = "",
        namespace: str = "",
        progress: Callable[[int], None] | None = None,
    ) -> Path:
        path = self._cache_dir(dir_suffix, salt, namespace)
        # Marker file written last to signal a complete extraction. Using a
        # marker (instead of just `path.is_dir()`) means an interrupted
        # extraction is correctly detected and re-run on the next invocation,
        # and lets us extract directly into ``path`` — avoiding a
        # post-extraction rename that races with antivirus on Windows.
        extracted_marker = path / _EXTRACTED_MARKER
        if not extracted_marker.is_file() or force:
            rmdir(path, msg=f"Clean up library directory {path}")

            if progress is None:
                # A batch caller draws one combined bar and logs the list
                _LOGGER.info("Downloading %s ...", self.url)
            _LOGGER.debug("Location: %s", path)

            # The sibling archive path lets an interrupted download's .part
            # file survive and resume on the next esphome run.
            download_and_extract(
                [self.url],
                {},
                path.with_name(f"{path.name}.archive"),
                path,
                progress=progress,
            )
            extracted_marker.touch()
        return path

    def __str__(self):
        return self.url


class GitSource(Source):
    def __init__(self, url: str, ref: str | None):
        self.url = url
        self.ref = ref

    def download(
        self, dir_suffix: str, force: bool = False, salt: str = "", namespace: str = ""
    ) -> Path:
        domain = DOMAIN
        if namespace:
            domain = f"{domain}/{namespace}"
        if salt:
            domain = f"{domain}/{salt}"
        path, _ = git.clone_or_update(
            url=self.url,
            ref=self.ref,
            refresh=git.NEVER_REFRESH if not force else None,
            domain=domain,
            init_submodules=True,
            subpath=Path(dir_suffix),
        )
        return path

    def __str__(self):
        return f"{self.url}#{self.ref}" if self.ref else self.url


class LocalSource(Source):
    """A library that already exists as a directory on the local filesystem.

    Referenced with a ``file://`` URL (PlatformIO's spelling for a local library
    folder). Nothing is copied: the backend generates its build files into an
    otherwise empty cache directory and references the library's own sources in
    place by absolute path (via :meth:`source_root`). So the user's source tree
    stays untouched and edits are picked up on the next build without syncing.
    """

    def __init__(self, path: str):
        self.local_path = path

    def download(
        self, dir_suffix: str, force: bool = False, salt: str = "", namespace: str = ""
    ) -> Path:
        src = Path(self.local_path)
        if not src.is_dir():
            # EsphomeError (not InvalidLibrary) so the CLI prints a clean message
            # instead of a traceback -- pointing a file:// at a missing folder is
            # the most common first mistake with a local library.
            raise EsphomeError(
                f"Local library directory does not exist: {self.local_path}"
            )
        base_dir = Path(CORE.data_dir) / DOMAIN
        if namespace:
            base_dir = base_dir / namespace
        h = hashlib.new("sha256")
        h.update(str(src.resolve()).encode())
        if salt:
            h.update(salt.encode())
        # Only the generated build files live here; the library's own sources
        # are referenced in place from source_root().
        path = base_dir / h.hexdigest()[:8] / dir_suffix
        path.mkdir(parents=True, exist_ok=True)
        return path

    def source_root(self, build_path: Path) -> Path:
        return Path(self.local_path)

    def __str__(self):
        path = Path(self.local_path)
        # as_uri() needs an absolute path; _node_key rejects relative file://
        # URLs, but guard anyway so a diagnostic can't itself raise.
        return path.as_uri() if path.is_absolute() else f"file://{self.local_path}"


class InvalidLibrary(Exception):
    pass


class IncompatiblePlatform(InvalidLibrary):
    """The routine cross-platform skip, typed so callers need not match
    message text."""


class ConvertedLibrary:
    """A resolved PlatformIO library plus its parsed manifest and on-disk path.

    Toolchain-neutral: ESP-IDF treats it as a component, Zephyr as a module. The
    backend reads ``name``/``version``/``data``/``dependencies``/``path`` to emit
    its build files.
    """

    def __init__(self, name: str, version: str, source: Source | None):
        self.name = name
        self.version = version
        self.source = source
        self.data = {}
        self.dependencies: list[ConvertedLibrary] = []
        self._path: Path | None = None
        # Where the library's own files live (manifest + sources). Set by
        # download(); equals path for registry/git, the user's dir for local.
        self.source_path: Path | None = None

    def __str__(self):
        return f"{self.name}@{self.version}={self.source}"

    @property
    def path(self) -> Path:
        if self._path is None:
            raise RuntimeError(f"path not set for library {self}")
        return self._path

    @path.setter
    def path(self, value: Path) -> None:
        self._path = value

    @property
    def source_dir(self) -> Path:
        """Directory the library's own files (manifest + sources) are read from.

        The build dir for a registry/git source; the user's directory for a
        local library. Backends read sources from here and emit their build
        files into ``path``.
        """
        return self.source_path or self.path

    def get_sanitized_name(self):
        return re.sub(r"[^a-zA-Z0-9_.\-/]", "_", self.name)

    def get_require_name(self):
        return self.get_sanitized_name().replace("/", "__")

    def download(self, force: bool = False, salt: str = "", namespace: str = ""):
        """Fetch the library into the shared cache and record its ``path``.

        The cache directory is named after the sanitized library name; backends
        rely on that name to identify the unit they build (e.g. ESP-IDF uses the
        directory name as the component name, replacing ``/`` with ``__`` via
        ``get_require_name``). ``namespace`` keeps each backend's cache separate.
        """
        self.path = self.source.download(
            self.get_sanitized_name(), force=force, salt=salt, namespace=namespace
        )
        self.source_path = self.source.source_root(self.path)


@dataclass
class LibraryBackend:
    """Toolchain hooks for :func:`convert_libraries`.

    ``platform``/``framework`` drive the manifest compatibility check.
    ``emit`` writes the toolchain-specific build files into a resolved library's
    ``path`` (e.g. the ESP-IDF ``CMakeLists.txt`` + ``idf_component.yml``, or a
    Zephyr ``module.yml`` + ``CMakeLists.txt``).
    ``cache_key`` namespaces the download cache (``pio_components/<cache_key>/``)
    so the differing build files two backends emit into a library dir never
    collide when the same config dir hosts both an ESP-IDF and a Zephyr build.
    """

    platform: str | None
    framework: str
    emit: Callable[["ConvertedLibrary"], None]
    cache_key: str


def ensure_list[T](obj: T | list[T]) -> list[T]:
    """
    Convert an object to a list if it isn't already a list.

    Args:
        obj: Object that may or may not already be a list.

    Returns:
        list[T]: The original list if ``obj`` is a list, otherwise a single-item
        list containing ``obj``.
    """
    return [obj] if not isinstance(obj, list) else obj


def _owner_pkgname_to_name(owner: str | None, pkgname: str) -> str:
    """
    Convert owner and package name to a standardized component name.

    This function combines owner and package name with a forward slash when
    both are provided, otherwise returns just the package name.

    Args:
        owner: The owner/username of the package (can be None)
        pkgname: The name of the package

    Returns:
        str: The standardized component name in "owner/pkgname" format or just "pkgname"
    """
    return f"{owner}/{pkgname}" if owner else pkgname


def collect_filtered_files(src_dir: PathType, src_filters: list[str]) -> list[str]:
    """
    Recursively match files in a directory according to include/exclude patterns.

    This function processes a list of filter strings that indicate which files
    to include or exclude. Each filter is parsed into patterns with a sign:
    '+' for inclusion and '-' for exclusion. Directory patterns ending with '/'
    are normalized to include all their contents recursively.

    Args:
        src_dir (PathType): Root directory to search within.
        src_filters (list[str]): List of filter strings, which may contain multiple
            patterns. Each pattern can start with '+' or '-' to indicate inclusion
            or exclusion.

    Returns:
        list[str]: List of matched file paths as strings. Only files (not directories)
        are returned, even if a directory matches a pattern.
    """
    matches = list(
        itertools.chain.from_iterable(
            FILTER_REGEX.findall(src_filter) for src_filter in src_filters
        )
    )

    selected = set()

    for sign, pattern in matches:
        pattern = pattern.strip()

        if pattern.endswith("/"):
            pattern = pattern.rstrip("/") + "/**"

        # glob.escape has no pathlib equivalent and the matcher works on raw
        # path strings, so PTH118/PTH207 don't apply here.
        full_pattern = os.path.join(glob.escape(str(src_dir)), pattern)  # noqa: PTH118

        matched = []
        for item in glob.glob(full_pattern, recursive=True):  # noqa: PTH207
            if not Path(item).is_dir():
                matched.append(item)
            else:
                # PlatformIO quirk: a directory matched with "*" should include all its
                # nested files and subdirectories, not just the directory itself.
                for root, _, files in os.walk(item):
                    matched.extend([str(Path(root) / f) for f in files])

        # glob keeps the pattern's literal separators for non-wildcard path
        # components, so on Windows the same file can surface with different
        # separators depending on where the wildcards sit; normalize so the
        # include/exclude set operations below compare equal paths.
        matched = [os.path.normpath(m) for m in matched]

        # FILTER_REGEX only ever captures "+" or "-", so the else is the "-" case.
        if sign == "+":
            selected.update(matched)
        else:
            selected.difference_update(matched)

    return [r for r in selected if Path(r).is_file()]


def split_list_by_condition(
    items: list[str], match_fn: Callable[[str], str | None]
) -> tuple[list[str], list[str]]:
    """
    Splits a list into two lists based on a matching function.

    Args:
        items: List of items to split.
        match_fn: Function that returns a value for items that should go into the "matched" list.

    Returns:
        A tuple (matched, non_matched)
    """
    matched = []
    non_matched = []
    for item in items:
        result = match_fn(item)
        if result:
            matched.append(result)
        else:
            non_matched.append(item)
    return matched, non_matched


def _valid_manifest_shape(data: Any) -> bool:
    """Whether the manifest has the dict shapes every backend dereferences.

    A bare json.load imposes no shape; validating once here means a
    malformed third-party manifest fails by library name instead of a raw
    TypeError/AttributeError in a backend.
    """
    if not isinstance(data, dict):
        return False
    build = data.get("build", {})
    esphome_data = data.get(ESPHOME_DATA_KEY, {})
    return (
        isinstance(build, dict)
        and isinstance(esphome_data, dict)
        and isinstance(esphome_data.get(ESPHOME_DATA_LINK_FLAGS_KEY, []), list)
        and isinstance(build.get("srcDir", ""), str)
        and isinstance(build.get("includeDir", ""), str)
        and isinstance(build.get("srcFilter", ""), (str, list))
    )


def check_library_data(data: dict, platform: str | None, framework: str):
    """
    Check whether a library manifest is compatible with the target toolchain.

    A platform mismatch (e.g. an AVR-only library on ESP32) raises
    ``InvalidLibrary`` so the caller skips the library. A framework mismatch only
    logs a warning — PIO manifests often understate the frameworks they actually
    compile under, and there's no opt-out at this layer, so we include the library
    anyway.

    Args:
        data: PIO library manifest dict being processed.
        platform: The PlatformIO platform token the build targets (e.g.
            ``espressif32``). ``None`` skips the platform check entirely — useful
            for targets (e.g. Zephyr) where PIO manifests rarely declare the
            platform yet portable libraries still build.
        framework: The active framework name (e.g. ``espidf``, ``arduino``,
            ``zephyr``) the manifest is expected to declare.

    Raises:
        InvalidLibrary: If the library does not support the target platform.
    """
    platforms = data.get("platforms", "*")
    if isinstance(platforms, str):
        platforms = [a.strip() for a in platforms.split(",")]
    platforms = ensure_list(platforms)
    if not all(isinstance(pf, str) for pf in platforms):
        # A real (non-platform) manifest problem; callers warn, not skip
        raise InvalidLibrary(f"Malformed platforms value: {platforms!r}")

    # Check if library supports the target platform
    valid_platforms = platform is None or "*" in platforms or platform in platforms

    if not valid_platforms:
        raise IncompatiblePlatform(f"Unsupported library platforms: {platforms}")

    frameworks = data.get("frameworks", "*")
    if isinstance(frameworks, str):
        frameworks = [a.strip() for a in frameworks.split(",")]
    frameworks = ensure_list(frameworks)
    if not all(isinstance(fw, str) for fw in frameworks):
        raise InvalidLibrary(f"Malformed frameworks value: {frameworks!r}")

    # Check if library declares the active framework. PIO library manifests
    # often list only "arduino" even when the library actually compiles fine
    # under the target framework, and there's no way to opt out of the check at
    # this layer. Warn instead of failing so the user isn't forced to fork the
    # library to fix the manifest.
    valid_framework = "*" in frameworks or framework in frameworks

    if not valid_framework:
        _LOGGER.warning(
            "Library %s declares frameworks %s that do not include '%s'; including anyway",
            data.get("name", "<unknown>"),
            frameworks,
            framework,
        )


def parse_library_json(library_json_path: PathType):
    """
    Load and parse a JSON file describing a library.

    Args:
        library_json_path (PathType): Path to the JSON file.

    Returns:
        dict: Parsed JSON content as a Python dictionary.
    """
    with Path(library_json_path).open(encoding="utf8") as fp:
        return json.load(fp)


def parse_library_properties(library_properties_path: PathType):
    """
    Parse a key-value platformio .properties style file into a dictionary.

    Args:
        library_properties_path (PathType): Path to the properties file.

    Returns:
        dict[str, str]: Mapping of parsed property keys to values.
    """
    with Path(library_properties_path).open(encoding="utf8") as fp:
        data = {}
        for line in fp.read().splitlines():
            line = line.strip()
            if not line or "=" not in line:
                continue
            # skip comments
            if line.startswith("#"):
                continue
            key, value = line.split("=", 1)
            if not value.strip():
                continue
            data[key.strip()] = value.strip()
        return data


def _make_registry_client() -> Any:
    """Create a minimal PlatformIO registry client with no system filtering.

    ``is_system_compatible`` is forced True so version selection is driven purely
    by the requested version requirements -- target compatibility is handled
    elsewhere, not by the PlatformIO registry.
    """
    from platformio.package.manager._registry import PackageManagerRegistryMixin

    class _Registry(PackageManagerRegistryMixin):
        def __init__(self) -> None:
            self._registry_client = None
            self.pkg_type = "library"

        @staticmethod
        def is_system_compatible(value: Any, custom_system: Any = None) -> bool:
            return True

    return _Registry()


def _resolve_registry_version(
    owner: str | None, pkgname: str, requirements: set[str]
) -> tuple[str, str, str, str, int | None]:
    """Resolve a registry package to the single highest version satisfying ALL
    the given requirements; return ``(owner, name, version, download_url,
    size)`` (``size`` is None when the registry omits it).

    Intersecting every requirement (rather than resolving each consumer in
    isolation) makes the result independent of processing order and guarantees
    no stated constraint is violated -- e.g. ``esphome/libsodium`` requested as
    both ``==1.10021.0`` and ``^1.10018.1`` resolves to ``1.10021.0``.
    """
    from platformio.package.meta import PackageSpec

    registry = _make_registry_client()
    package = registry.fetch_registry_package(PackageSpec(owner=owner, name=pkgname))
    owner = package["owner"]["username"]
    name = package["name"]

    # Chaining the per-requirement filter intersects all constraints.
    versions = package.get("versions") or []
    for requirement in sorted(requirements):
        versions = registry.get_compatible_registry_versions(
            versions, PackageSpec(owner=owner, name=name, requirements=requirement)
        )
    if not versions:
        raise RuntimeError(
            f"No version of {owner}/{name} satisfies all requirements "
            f"{sorted(requirements)} requested across the library tree"
        )

    best = registry.pick_best_registry_version(versions)
    pkgfile = registry.pick_compatible_pkg_file(best["files"])
    if not pkgfile:
        raise RuntimeError(f"No package file for {owner}/{name}@{best['name']}")
    return owner, name, best["name"], pkgfile["download_url"], pkgfile.get("size")


def split_flag_entry(entry: Any, owner: str) -> list[str]:
    """``shlex.split`` with a clean error naming the offending flags entry."""
    # Late import: shlex is only needed when actually lexing flags
    import shlex

    try:
        return shlex.split(entry)
    except (ValueError, AttributeError, TypeError) as err:
        # AttributeError/TypeError: a dict or number from a third-party
        # manifest; name the entry instead of an opaque shlex traceback
        raise EsphomeError(f"Malformed build flag {entry!r} in {owner}: {err}") from err


def lex_build_flags(entries: str | list[str], owner: str) -> list[str]:
    """Shell-lex ``build.flags`` entries the way PlatformIO's ParseFlags
    does; bare -I/-L/-l/-D tokens re-glue to their argument."""
    # Lex per entry as ParseFlags does: a dangling -I must warn, not absorb
    # the next entry's first token
    return [
        token
        for entry in ensure_list(entries)
        for token in join_flag_args(split_flag_entry(entry, owner), owner)
    ]


# Flags whose argument may follow as a separate token; ParseFlags glues them
BARE_ARG_FLAGS = frozenset({"-I", "-L", "-l", "-D"})


def join_flag_args(tokens: Iterable[str], owner: str) -> list[str]:
    """Join a bare ``-I``/``-L``/``-l``/``-D`` with its following token, as
    PlatformIO's ParseFlags does. A trailing or empty argument is warned and
    dropped: the bare flag would make gcc eat the next flag."""
    out: list[str] = []
    it = iter(tokens)
    for tok in it:
        if tok in BARE_ARG_FLAGS:
            arg = next(it, None)
            if arg is None:
                _LOGGER.warning("Ignoring trailing '%s' in %s build flags", tok, owner)
                break
            if not arg:
                _LOGGER.warning(
                    "Ignoring '%s' with empty argument in %s build flags", tok, owner
                )
                continue
            tok += arg
        out.append(tok)
    return out


def warn_properties_depends(name: str, data: object) -> None:
    """Warn for ``depends=``-only manifests; the walk reads only the JSON
    ``dependencies`` key, so they would otherwise drop silently."""
    if isinstance(data, dict) and not data.get("dependencies") and data.get("depends"):
        # INFO: common and unactionable for transitive libraries; a WARNING
        # on every build would train users to ignore the stream
        _LOGGER.info(
            "Library %s declares dependencies via library.properties "
            "depends=, which are not resolved automatically; add them with "
            "add_library() if needed",
            name,
        )


def dependency_is_usable(
    dep: dict, platform: str | None, framework: str, requester: str
) -> bool:
    """Compatibility filter for a manifest dependency: platform mismatches
    skip at debug, any other ``InvalidLibrary`` warns naming the requester."""
    try:
        check_library_data(dep, platform, framework)
    except IncompatiblePlatform as e:
        _LOGGER.debug("Skip dependency %s of %s: %s", dep.get("name"), requester, e)
        return False
    except InvalidLibrary as e:
        _LOGGER.warning(
            "Skipping dependency %s of %s: %s", dep.get("name"), requester, e
        )
        return False
    return True


def _valid_dependency_entry(entry: dict, manifest_name: str) -> bool:
    """Whether a normalized entry carries a usable name (non-empty string),
    version (string, if present), and owner (string, if present); invalid
    entries warn naming the manifest."""
    name = entry.get("name")
    owner = entry.get("owner")
    name_ok = isinstance(name, str) and name
    version_ok = "version" not in entry or isinstance(entry["version"], str)
    owner_ok = owner is None or isinstance(owner, str)
    if name_ok and version_ok and owner_ok:
        return True
    _LOGGER.warning(
        "Ignoring unrecognized dependency entry %r of %s", entry, manifest_name
    )
    return False


def normalize_dependencies(
    dependencies: Any, manifest_name: str = "manifest"
) -> list[dict]:
    """Normalize a library manifest's ``dependencies`` to a list of dicts.

    PIO's library.json accepts the list-of-dicts form, the shorthand dict
    form (``{"owner/Name": "version_spec"}``), bare name strings inside the
    list, and a plain (possibly comma-separated) string; normalize them all
    so callers see a uniform list. ``manifest_name`` names the manifest in the
    warning for entries that cannot be normalized.

    Bare-name spellings carry no version; the dependency walk later drops
    version-less entries at DEBUG, since they are usually names of bundled
    framework libraries (Wire, SPI) that need no registry install.
    """
    if dependencies is None:
        return []
    if isinstance(dependencies, str):
        # A plain string is one or more comma-separated names; iterating it
        # as a list would shred it into one-character "libraries"
        return [{"name": n.strip()} for n in dependencies.split(",") if n.strip()]
    if isinstance(dependencies, dict):
        normalized = []
        for raw_name, spec in dependencies.items():
            if isinstance(raw_name, str) and "/" in raw_name:
                owner, pkgname = raw_name.split("/", 1)
            else:
                owner, pkgname = None, raw_name
            entry = {"name": pkgname, "owner": owner}
            if isinstance(spec, dict):
                entry.update(spec)
            else:
                entry["version"] = spec
            if _valid_dependency_entry(entry, manifest_name):
                normalized.append(entry)
        return normalized
    if not isinstance(dependencies, (list, tuple)):
        _LOGGER.warning(
            "Ignoring unrecognized dependencies %r of %s",
            dependencies,
            manifest_name,
        )
        return []
    normalized = []
    for entry in dependencies:
        if isinstance(entry, dict):
            if _valid_dependency_entry(entry, manifest_name):
                normalized.append(entry)
        elif isinstance(entry, str) and entry:
            # PIO also accepts a bare list of names ("dependencies": ["Wire"])
            normalized.append({"name": entry})
        else:
            _LOGGER.warning(
                "Ignoring unrecognized dependency entry %r of %s",
                entry,
                manifest_name,
            )
    return normalized


@dataclass
class _LibNode:
    """A node in the library dependency graph being resolved as a batch."""

    key: str
    is_git: bool
    is_local: bool = False
    is_registry: bool = False
    owner: str | None = None
    pkgname: str | None = None
    requirements: set[str] = field(default_factory=set)
    url: str | None = None
    ref: str | None = None
    local_path: str | None = None
    edges: set[str] = field(default_factory=set)


def _url_or_none(value: Any) -> str | None:
    """Return ``value`` if it parses as a URL (scheme and host), else None."""
    if not value or not isinstance(value, str):
        return None
    try:
        parsed = urlsplit(value)
    except ValueError:
        return None
    return value if parsed.scheme and parsed.netloc else None


def _node_key(
    name: str | None, version: str | None, repository: str | None
) -> tuple[str, str, tuple[str | None, str | None]]:
    """Return ``(key, kind, locator)`` for a library or dependency spec.

    ``kind`` is one of:

    - ``"registry"`` -- ``locator`` is ``(owner, pkgname)``.
    - ``"git"`` -- ``locator`` is ``(url, ref)``.
    - ``"local"`` -- a ``file://`` directory; ``locator`` is ``(path, None)``.

    The key is derived from the *input* spec (the registry name as written, the
    git URL path, or the custom name / directory name for a local folder), not
    the resolved canonical name. So a package referenced inconsistently -- bare
    ``name`` vs ``owner/name``, or git vs registry -- maps to distinct keys and
    isn't deduplicated; ``convert_libraries`` warns about that after resolution
    rather than merging the nodes.

    PlatformIO's Library Manager also accepted a URL in the *name* position
    (``add_library("https://github.com/x/y", None)``), including the ``git+``
    VCS prefix and the ``CustomName=URL`` form; recognize those here so such
    specs resolve as git (or local) sources instead of failing a registry
    lookup. A plain ``file://`` URL is PlatformIO's spelling for a local library
    folder, so it resolves as a local directory; ``git+file://`` stays a git
    source.
    """
    if not repository and name and "://" in name:
        # Split a ``CustomName=URL`` name, but only when the whole string isn't
        # itself a valid URL (a bare URL whose query contains ``=`` must stay
        # intact).
        custom_name, candidate = None, name
        if "=" in name and _url_or_none(name) is None:
            custom_name, candidate = name.split("=", 1)
        try:
            scheme = urlsplit(candidate).scheme
        except ValueError:
            scheme = ""
        if scheme == "file" or _url_or_none(candidate):
            name, repository = custom_name, candidate
        else:
            # Anything with ``://`` was meant to be a URL; failing it fast
            # beats a confusing registry "package not found" error.
            raise RuntimeError(f"Invalid PIO library URL: {name}")
    if repository:
        is_git_prefixed = repository.startswith("git+")
        split_result = urlsplit(repository.removeprefix("git+"))
        if split_result.scheme == "file" and not is_git_prefixed:
            # A plain file:// URL points at a local library directory. A local
            # file URL is written file:///absolute/path (empty host) or, less
            # commonly, file://localhost/path. Anything else -- a real host, or
            # a relative path whose first segment parses as the host -- is
            # rejected rather than silently resolved to the wrong directory.
            if split_result.netloc not in ("", "localhost"):
                raise RuntimeError(
                    f"Unsupported host in file:// library URL '{repository}'; "
                    "use an absolute path, e.g. file:///path/to/lib"
                )
            # Validate the URL path itself (always POSIX-style, leading slash),
            # not the OS path: on Windows a "/foo" path is not is_absolute()
            # without a drive, which would wrongly reject a valid file:/// URL.
            # Reject a relative path (``file:lib_dev``) or a bare root
            # (``file:///``, which has no final segment).
            url_path = split_result.path
            if not url_path.startswith("/") or not PurePosixPath(url_path).name:
                raise RuntimeError(
                    f"file:// library URL '{repository}' must be an absolute "
                    "directory path, e.g. file:///path/to/lib"
                )
            path = url2pathname(url_path)
            return (name or PurePosixPath(url_path).name), "local", (path, None)
        key = str(split_result.path).strip("/").removesuffix(".git")
        ref = split_result.fragment.strip() or None
        url = urlunsplit(split_result._replace(fragment=""))
        return key, "git", (url, ref)
    if name and "/" in name:
        owner, pkgname = name.split("/", 1)
    else:
        owner, pkgname = None, name
    return name, "registry", (owner, pkgname)


def lib_ignore_set() -> set[str]:
    """The ``lib_ignore`` names from ``esphome->platformio_options``,
    normalized to lowercase short names (the part after the ``/``)."""
    return {
        name.split("/")[-1].lower()
        for name in CORE.platformio_options.get("lib_ignore", [])
    }


def is_lib_ignored(name: str | None, lib_ignore: set[str]) -> bool:
    """Whether ``name`` matches the normalized ``lib_ignore`` set."""
    return (
        bool(lib_ignore)
        and name is not None
        and (name.split("/")[-1].lower() in lib_ignore)
    )


def _fetch_source(
    component: ConvertedLibrary,
    salt: str,
    namespace: str,
    tracker: Callable[[int], None],
) -> None:
    # Straight to URLSource: only it takes progress, and mutating the
    # shared component from a worker is the authoritative loop's job
    component.source.download(
        component.get_sanitized_name(), salt=salt, namespace=namespace, progress=tracker
    )


def _prefetch_wave(
    wave: list[tuple[str, ConvertedLibrary]], salt: str, namespace: str
) -> None:
    """Best-effort parallel download of a wave's registry archives.

    The walk's own ``download()`` stays authoritative; duplicate URLs
    prefetch once so two threads never share a cache directory. Archives
    whose size the registry did not report are left to the sequential
    loop, whose per-file bars don't interleave. A node a sibling in the
    same wave supersedes has its archive fetched in vain (knowing better
    would need the manifests being downloaded).
    """
    try:
        components: list[ConvertedLibrary] = []
        seen: set[str] = set()
        for _key, component in wave:
            source = component.source
            if not isinstance(source, URLSource) or not source.size:
                continue
            if source.url in seen:
                continue
            seen.add(source.url)
            try:
                cached = source.is_cached(
                    component.get_sanitized_name(), salt=salt, namespace=namespace
                )
            except OSError as err:
                # Best-effort, but visibly: a systematic probe failure makes
                # every warm build re-download every archive
                _LOGGER.warning("Cache probe for %s failed: %s", component.name, err)
                cached = False
            if cached:
                # A warm build must stay silent
                continue
            components.append(component)
        if not components:
            return
        # Single-item waves (a dependency chain discovers one archive per
        # wave) go through the same runner: one download method, one bar
        _LOGGER.info(
            "Downloading %d library archive(s): %s",
            len(components),
            ", ".join(c.name for c in components),
        )
        failures = run_batch_downloads(
            "Downloading libraries",
            [
                (c.name, c.source.size, partial(_fetch_source, c, salt, namespace))
                for c in components
            ],
        )
        # The sequential call below retries and raises the real error
        warn_prefetch_failures(
            failures, "Prefetch of %s failed (retrying sequentially): %s"
        )
    except Exception as err:  # noqa: BLE001  # pylint: disable=broad-exception-caught
        # Same policy as the ESP-IDF twin: the prefetch must never become a
        # new way for the build to fail
        _LOGGER.warning("Library prefetch failed: %s", failure_reason(err))
        _LOGGER.debug("Prefetch failure detail", exc_info=True)


def convert_libraries(
    libraries: list[Library], backend: LibraryBackend
) -> list[ConvertedLibrary]:
    """Resolve and convert a batch of PlatformIO libraries for ``backend``.

    Resolves the whole set together rather than each library independently: it
    walks the dependency graph collecting every version *requirement* per
    component name, then resolves each name once to a single version satisfying
    all of them. So a transitive dependency shared under
    different specs (e.g. ``esphome/libsodium``, pulled by both ``noise-c`` and
    ``esp_wireguard``) becomes one component instead of two clashing
    ``override_path`` entries -- order-independently, and without ever violating
    a stated constraint.

    The returned list holds the top-level components (those directly requested);
    transitive dependencies are converted too and wired into each component's
    generated manifest. ``backend.emit`` is called once per converted library to
    write its toolchain-specific build files.

    ``lib_ignore`` from ``esphome->platformio_options`` excludes libraries by
    short name (part after the ``/``), matched against both the top-level
    libraries and every dependency discovered during the graph walk.
    """
    nodes: dict[str, _LibNode] = {}

    lib_ignore = lib_ignore_set()

    # The generated build files inside the shared cache bake in the dependency
    # wiring, which lib_ignore changes; salt the cache path so configs with
    # different lib_ignore values don't fight over (and constantly rewrite) the
    # same converted component files.
    salt = (
        hashlib.sha256(",".join(sorted(lib_ignore)).encode()).hexdigest()[:8]
        if lib_ignore
        else ""
    )

    def add_spec(name: str | None, version: str | None, repository: str | None) -> str:
        key, kind, locator = _node_key(name, version, repository)
        node = nodes.get(key) or _LibNode(key=key, is_git=kind == "git")
        nodes[key] = node
        # The same key requested from two different kinds of source (or two
        # different local paths) is a config mistake: one silently wins. Warn so
        # it isn't a surprise. (git-vs-registry is reported separately below.)
        if kind == "git":
            if node.is_local:
                _LOGGER.warning(
                    "Library %s is requested as both a local directory and a git "
                    "source; using the git source.",
                    key,
                )
            node.is_git = True
            node.url, node.ref = locator
        elif kind == "local":
            new_path = locator[0]
            if node.is_git:
                # git wins (checked first when building the source); leave the
                # node as a git source.
                _LOGGER.warning(
                    "Library %s is requested as both a local directory and a git "
                    "source; using the git source.",
                    key,
                )
            else:
                if node.is_local and node.local_path != new_path:
                    _LOGGER.warning(
                        "Library %s is requested from two local directories (%s "
                        "and %s); using %s.",
                        key,
                        node.local_path,
                        new_path,
                        new_path,
                    )
                node.is_local = True
                node.local_path = new_path
        else:
            node.is_registry = True
            node.owner, node.pkgname = locator
            if version:
                node.requirements.add(version)
        return key

    top_level = [
        add_spec(library.name, library.version, library.repository)
        for library in libraries
        if not is_lib_ignored(library.name, lib_ignore)
    ]

    # Collect + resolve to a fixpoint: a node is (re)resolved whenever its
    # requirement set has grown since the last time, so every requirement in the
    # graph is accounted for before conversion.
    components: dict[str, ConvertedLibrary] = {}
    resolved_requirements: dict[str, frozenset[str]] = {}
    top_level_keys = set(top_level)
    worklist = deque(dict.fromkeys(top_level))
    while worklist:
        # Drain the frontier sequentially (spec resolution mutates shared
        # state), then prefetch the wave in parallel
        wave: list[tuple[str, ConvertedLibrary]] = []
        while worklist:
            key = worklist.popleft()
            node = nodes[key]

            # Re-resolve only when the requirement set grew; requirements
            # only ever grow, so the fixpoint converges and cycles terminate
            requirements = frozenset(node.requirements)
            if resolved_requirements.get(key) == requirements:
                continue
            resolved_requirements[key] = requirements

            if node.is_git:
                component = ConvertedLibrary(key, "*", GitSource(node.url, node.ref))
            elif node.is_local:
                component = ConvertedLibrary(key, "*", LocalSource(node.local_path))
            else:
                owner, name, version, url, size = _resolve_registry_version(
                    node.owner, node.pkgname, node.requirements
                )
                component = ConvertedLibrary(
                    _owner_pkgname_to_name(owner, name), version, URLSource(url, size)
                )
            wave.append((key, component))
        _prefetch_wave(wave, salt, backend.cache_key)
        for key, component in wave:
            node = nodes[key]
            if frozenset(node.requirements) != resolved_requirements[key]:
                # Requirements grew mid-wave: skip parsing a manifest the
                # next wave will re-resolve and replace
                worklist.append(key)
                continue
            component.download(salt=salt, namespace=backend.cache_key)

            source_dir = component.source_dir
            library_json_path = source_dir / "library.json"
            library_properties_path = source_dir / "library.properties"
            has_json = library_json_path.is_file()
            has_properties = library_properties_path.is_file()
            if not has_json and not has_properties and not node.is_local:
                # An interrupted clone/extraction self-heals with one forced
                # re-download; a local source has nothing to re-download
                _LOGGER.warning(
                    "Library %s at %s is missing library.json and library.properties; "
                    "re-downloading",
                    key,
                    source_dir,
                )
                component.download(force=True, salt=salt, namespace=backend.cache_key)
                has_json = library_json_path.is_file()
                has_properties = library_properties_path.is_file()
            if has_json:
                component.data = parse_library_json(library_json_path)
            elif has_properties:
                component.data = parse_library_properties(library_properties_path)
            else:
                # Local sources are user input (EsphomeError); a registry/git
                # miss means a corrupt cache (RuntimeError)
                error_cls = EsphomeError if node.is_local else RuntimeError
                raise error_cls(
                    f"Invalid PIO library {key}: missing library.json and "
                    f"library.properties in {source_dir}"
                )

            if not _valid_manifest_shape(component.data):
                # Fail fast only for a library the user asked for; a defect
                # in an unrequested corner of the graph must not block the
                # build
                if key in top_level_keys:
                    raise EsphomeError(f"Library {key} has a malformed manifest")
                _LOGGER.warning("Skipping dependency %s: malformed manifest", key)
                continue
            warn_properties_depends(component.name, component.data)

            try:
                check_library_data(component.data, backend.platform, backend.framework)
            except InvalidLibrary as e:
                # An explicitly requested library fails fast; the routine
                # cross-platform skip stays at debug, other causes warn
                if key in top_level_keys:
                    reason = (
                        f"is not compatible with {backend.framework}"
                        if isinstance(e, IncompatiblePlatform)
                        else "has a malformed manifest"
                    )
                    raise RuntimeError(f"Requested library {key} {reason}: {e}") from e
                if isinstance(e, IncompatiblePlatform):
                    _LOGGER.debug("Skip incompatible dependency %s: %s", key, str(e))
                else:
                    _LOGGER.warning("Skipping dependency %s: %s", key, str(e))
                continue
            components[key] = component

            # Requirements changed (we got past the short-circuit above), so
            # (re)walk this component's dependencies.
            node.edges = set()
            for dependency in normalize_dependencies(
                component.data.get("dependencies"), component.name
            ):
                if "version" not in dependency:
                    # Cannot resolve from the registry; common for bundled
                    # names (Wire, SPI) -- unactionable noise above debug
                    _LOGGER.debug(
                        "Skip version-less dependency %r of %s",
                        dependency.get("name"),
                        component.name,
                    )
                    continue
                if not dependency_is_usable(
                    dependency, backend.platform, backend.framework, component.name
                ):
                    continue
                dep_name = _owner_pkgname_to_name(
                    dependency.get("owner"), dependency.get("name")
                )
                if is_lib_ignored(dep_name, lib_ignore):
                    _LOGGER.debug("Skip ignored dependency %s", dep_name)
                    continue
                # The version field may actually be a URL (git/archive dependency).
                dep_version = dependency["version"]
                dep_url = _url_or_none(dep_version)
                if dep_url is not None:
                    dep_version = None
                dep_key = add_spec(dep_name, dep_version, dep_url)
                node.edges.add(dep_key)
                worklist.append(dep_key)

    # A git or local source wins over the same component requested from the
    # registry. That's intentional, but warn so the dropped registry spec isn't
    # a silent surprise -- including when it carried no version pin (a bare
    # cg.add_library("Foo"), which is how most components add libraries).
    for node in nodes.values():
        if (node.is_git or node.is_local) and (node.is_registry or node.requirements):
            source = "git" if node.is_git else "local"
            registry = (
                f"registry version(s) {sorted(node.requirements)}"
                if node.requirements
                else "a registry package"
            )
            _LOGGER.warning(
                "Library %s is requested both from a %s source (%s) and as %s; "
                "using the %s source.",
                node.key,
                source,
                node.url if node.is_git else node.local_path,
                registry,
                source,
            )

    # Two graph nodes that resolve to the same component name (e.g. a package
    # referenced both bare and as ``owner/name``) are not deduplicated and can
    # produce conflicting component definitions. Warn so it's not silent.
    canonical_keys: dict[str, str] = {}
    for node_key, component in components.items():
        canonical = component.get_sanitized_name()
        if canonical_keys.setdefault(canonical, node_key) != node_key:
            _LOGGER.warning(
                "Library %s is referenced under multiple names (%s and %s); these "
                "are not deduplicated. Reference it consistently as %s.",
                canonical,
                canonical_keys[canonical],
                node_key,
                canonical,
            )

    # Wire each component's dependencies to the single resolved instances, then
    # emit build files.
    for key, component in components.items():
        component.dependencies = [
            components[dep_key]
            for dep_key in sorted(nodes[key].edges)
            if dep_key in components
        ]
    for component in components.values():
        backend.emit(component)

    return [components[key] for key in top_level if key in components]
