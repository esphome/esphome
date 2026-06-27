from collections import deque
from collections.abc import Callable
from dataclasses import dataclass, field
import glob
import hashlib
import itertools
import json
import logging
import os
from pathlib import Path
import re
import tempfile
from typing import Any, TypeVar
from urllib.parse import urlparse, urlsplit, urlunsplit

from esphome import git, yaml_util
from esphome.core import CORE, Library
from esphome.espidf.framework import archive_extract_all, download_from_mirrors, rmdir
from esphome.helpers import write_file_if_changed

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
SRC_FILE_EXTENSIONS = [
    ".c",
    ".cpp",
    ".cc",
    ".cxx",
    ".c++",
    ".S",
    ".spp",
    ".SPP",
    ".sx",
    ".s",
    ".asm",
    ".ASM",
]

ESP32_PLATFORM = "espressif32"
DOMAIN = "pio_components"

ESPHOME_DATA_KEY = "ESPHOME"
ESPHOME_DATA_EXTRA_CMAKE_KEY = "EXTRA_CMAKE"


class Source:
    def download(self, dir_suffix: str, force: bool = False, salt: str = "") -> Path:
        raise NotImplementedError


class URLSource(Source):
    def __init__(self, url: str):
        self.url = url

    def download(self, dir_suffix: str, force: bool = False, salt: str = "") -> Path:
        base_dir = Path(CORE.data_dir) / DOMAIN
        h = hashlib.new("sha256")
        h.update(self.url.encode())
        if salt:
            h.update(salt.encode())
        path = base_dir / h.hexdigest()[:8] / dir_suffix
        # Marker file written last to signal a complete extraction. Using a
        # marker (instead of just `path.is_dir()`) means an interrupted
        # extraction is correctly detected and re-run on the next invocation,
        # and lets us extract directly into ``path`` — avoiding a
        # post-extraction rename that races with antivirus on Windows.
        extracted_marker = path / ".esphome_extracted"
        if not extracted_marker.is_file() or force:
            rmdir(path, msg=f"Clean up library directory {path}")

            # Download in temporary file
            with tempfile.NamedTemporaryFile() as tmp:
                _LOGGER.info("Downloading %s ...", self.url)
                _LOGGER.debug("Location: %s", path)

                download_from_mirrors([self.url], {}, tmp.file)

                _LOGGER.debug("Extracting archive to %s ...", path)
                archive_extract_all(tmp.file, path)
                extracted_marker.touch()
        return path

    def __str__(self):
        return self.url


class GitSource(Source):
    def __init__(self, url: str, ref: str | None):
        self.url = url
        self.ref = ref

    def download(self, dir_suffix: str, force: bool = False, salt: str = "") -> Path:
        path, _ = git.clone_or_update(
            url=self.url,
            ref=self.ref,
            refresh=git.NEVER_REFRESH if not force else None,
            domain=f"{DOMAIN}/{salt}" if salt else DOMAIN,
            submodules=[],
            subpath=Path(dir_suffix),
        )
        return path

    def __str__(self):
        return f"{self.url}#{self.ref}" if self.ref else self.url


class InvalidIDFComponent(Exception):
    pass


class IDFComponent:
    def __init__(self, name: str, version: str, source: Source | None):
        self.name = name
        self.version = version
        self.source = source
        self.data = {}
        self.dependencies: list[IDFComponent] = []
        self._path: Path | None = None

    def __str__(self):
        return f"{self.name}@{self.version}={self.source}"

    @property
    def path(self) -> Path:
        if self._path is None:
            raise RuntimeError(f"path not set for component {self}")
        return self._path

    @path.setter
    def path(self, value: Path) -> None:
        self._path = value

    def get_sanitized_name(self):
        return re.sub(r"[^a-zA-Z0-9_.\-/]", "_", self.name)

    def get_require_name(self):
        return self.get_sanitized_name().replace("/", "__")

    def download(self, force: bool = False, salt: str = ""):
        """
        The dependency name should match the directory name at the end of the override path.
        The ESP-IDF build system uses the directory name as the component name, so the directory of the override_path should match the component name.
        If you want to specify the full name of the component with the namespace, replace / in the component name with __.
        @see https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html
        """
        self.path = self.source.download(
            self.get_sanitized_name(), force=force, salt=salt
        )


def _apply_extra_script(component: IDFComponent) -> None:
    """Run a PIO ``extraScript`` and fold its captured env vars into
    ``component.data["build"]["flags"]`` so the existing -L/-l/-D
    extraction in ``generate_cmakelists_txt`` picks them up."""
    extra_script = component.data.get("build", {}).get("extraScript")
    if not extra_script:
        return
    # Resolve and confine to the component dir so a malicious library.json
    # can't escape (e.g. ``"extraScript": "../../etc/passwd"``).
    library_root = component.path.resolve()
    script_path = (component.path / extra_script).resolve()
    if not script_path.is_relative_to(library_root) or not script_path.is_file():
        return
    from esphome.components.esp32 import get_esp32_variant
    from esphome.espidf.extra_script import captured_as_build_flags, run_extra_script

    idf_target = get_esp32_variant().lower().replace("-", "")
    result = run_extra_script(
        script_path, library_dir=component.path, idf_target=idf_target
    )
    extra_flags = captured_as_build_flags(result, library_dir=component.path)
    if not extra_flags:
        return
    flags = component.data.setdefault("build", {}).setdefault("flags", [])
    if isinstance(flags, str):
        flags = [flags]
    flags.extend(extra_flags)
    component.data["build"]["flags"] = flags


T = TypeVar("T")


def _ensure_list(obj: T | list[T]) -> list[T]:
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


def _collect_filtered_files(src_dir: PathType, src_filters: list[str]) -> list[str]:
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

        if sign == "+":
            selected.update(matched)
        elif sign == "-":
            selected.difference_update(matched)

    return [r for r in selected if Path(r).is_file()]


def _split_list_by_condition(
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
        # In CMakeLists.txt, backslashes need to be escaped
        return f'"{str(p)}"'.replace("\\", "\\\\")

    # Extract the values
    build_src_dir = component.data.get("build", {}).get("srcDir", None)
    if not build_src_dir:
        for d in ["src", "Src", "."]:
            if (component.path / Path(d)).is_dir():
                build_src_dir = d
                break

    build_include_dir = component.data.get("build", {}).get(
        "includeDir", DEFAULT_BUILD_INCLUDE_DIR
    )
    build_src_filter = _ensure_list(
        component.data.get("build", {}).get("srcFilter", DEFAULT_BUILD_SRC_FILTER)
    )
    build_flags = _ensure_list(
        component.data.get("build", {}).get("flags", DEFAULT_BUILD_FLAGS)
    )

    # List all sources files
    build_src_files = _collect_filtered_files(
        component.path / Path(build_src_dir), build_src_filter
    )

    # Only bake library.json-declared deps here. Project-managed and
    # built-in components come in via ${ESPHOME_PROJECT_MANAGED_COMPONENTS}
    # / ${ESPHOME_PROJECT_BUILTIN_COMPONENTS} set in the top-level
    # CMakeLists, so this file stays project-agnostic when shared from
    # the pio_components cache.
    requires: set[str] = {
        dependency.get_require_name() for dependency in component.dependencies
    }

    # Only keep sources
    build_src_files = [os.path.relpath(p, component.path) for p in build_src_files]
    build_src_files = [
        f for f in build_src_files if Path(f).suffix in SRC_FILE_EXTENSIONS
    ]

    # Handle build flags
    include_dir_flags, build_flags = _split_list_by_condition(
        build_flags, lambda a: a[2:].strip() if a.startswith("-I") else None
    )
    link_directories, build_flags = _split_list_by_condition(
        build_flags, lambda a: a[2:].strip() if a.startswith("-L") else None
    )
    link_libraries, build_flags = _split_list_by_condition(
        build_flags, lambda a: a[2:].strip() if a.startswith("-l") else None
    )

    # Split include directories from build_flags
    # Only keep an include directory if it exists
    build_include_dirs = [build_include_dir, build_src_dir] + include_dir_flags
    build_include_dirs = [
        d for d in build_include_dirs if (component.path / Path(d)).is_dir()
    ]

    # Split build_flags list into private and public lists
    private_build_flags, public_build_flags = _split_list_by_condition(
        build_flags, lambda a: a if a.startswith("-W") else None
    )

    # Generate the component
    content = "idf_component_register(\n"
    if build_src_files:
        str_srcs = " ".join([escape_entry(p) for p in sorted(build_src_files)])
        content += f"  SRCS {str_srcs}\n"
    if build_include_dirs:
        str_include_dirs = " ".join([escape_entry(p) for p in build_include_dirs])
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
            str_build_flag = escape_entry(link_directory)
            content += f"  {str_build_flag}\n"
        content += ")\n"

    if link_libraries:
        content += "target_link_libraries(${COMPONENT_LIB} INTERFACE\n"
        for link_library in link_libraries:
            str_build_flag = escape_entry(link_library)
            content += f"  {str_build_flag}\n"
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
    data = {}

    description = component.data.get("description")
    if description:
        data["description"] = description

    repository = component.data.get("repository", {}).get("url", None)
    if repository:
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


def _check_library_data(data: dict):
    """
    Check if a library data is compatible with the ESP-IDF framework.

    A platform mismatch (e.g. an AVR-only library on ESP32) raises
    ``InvalidIDFComponent`` so the caller skips the library. A framework
    mismatch only logs a warning — PIO manifests often understate the
    frameworks they actually compile under, and IDF (unlike PIO's
    ``lib_compat_mode``) has no opt-out, so we include the library anyway.

    Args:
        data: PIO library manifest dict being processed.

    Raises:
        InvalidIDFComponent: If the library does not support the ESP32 platform.
    """
    platforms = data.get("platforms", "*")
    if isinstance(platforms, str):
        platforms = [a.strip() for a in platforms.split(",")]
    platforms = _ensure_list(platforms)

    # Check if library supports ESP-IDF platform
    valid_platforms = "*" in platforms or ESP32_PLATFORM in platforms

    if not valid_platforms:
        raise InvalidIDFComponent(f"Unsupported library platforms: {platforms}")

    frameworks = data.get("frameworks", "*")
    if isinstance(frameworks, str):
        frameworks = [a.strip() for a in frameworks.split(",")]
    frameworks = _ensure_list(frameworks)

    # Check if library declares the active framework. PIO library manifests
    # often list only "arduino" even when the library actually compiles fine
    # under ESP-IDF, and IDF (unlike PIO with `lib_compat_mode`) has no way to
    # opt out of the check. Warn instead of failing so the user isn't forced to
    # fork the library to fix the manifest.
    framework = "arduino" if CORE.using_arduino else "espidf"
    valid_framework = "*" in frameworks or framework in frameworks

    if not valid_framework:
        _LOGGER.warning(
            "Library %s declares frameworks %s that do not include '%s'; including anyway",
            data.get("name", "<unknown>"),
            frameworks,
            framework,
        )


def _parse_library_json(library_json_path: PathType):
    """
    Load and parse a JSON file describing a library.

    Args:
        library_json_path (PathType): Path to the JSON file.

    Returns:
        dict: Parsed JSON content as a Python dictionary.
    """
    with Path(library_json_path).open(encoding="utf8") as fp:
        return json.load(fp)


def _parse_library_properties(library_properties_path: PathType):
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
    by the requested version requirements -- ESP-IDF/target compatibility is
    handled elsewhere, not by the PlatformIO registry.
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
) -> tuple[str, str, str, str]:
    """Resolve a registry package to the single highest version satisfying ALL
    the given requirements; return ``(owner, name, version, download_url)``.

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
    return owner, name, best["name"], pkgfile["download_url"]


def _normalize_dependencies(dependencies: Any) -> list[dict]:
    """Normalize a library manifest's ``dependencies`` to a list of dicts.

    PIO's library.json accepts both the list-of-dicts form and the shorthand
    dict form (``{"owner/Name": "version_spec"}``); normalize the latter so
    callers see a uniform list.
    """
    if not dependencies:
        return []
    if isinstance(dependencies, dict):
        normalized = []
        for raw_name, spec in dependencies.items():
            if "/" in raw_name:
                owner, pkgname = raw_name.split("/", 1)
            else:
                owner, pkgname = None, raw_name
            entry = {"name": pkgname, "owner": owner}
            if isinstance(spec, dict):
                entry.update(spec)
            else:
                entry["version"] = spec
            normalized.append(entry)
        return normalized
    return [d for d in dependencies if isinstance(d, dict)]


@dataclass
class _LibNode:
    """A node in the library dependency graph being resolved as a batch."""

    key: str
    is_git: bool
    owner: str | None = None
    pkgname: str | None = None
    requirements: set[str] = field(default_factory=set)
    url: str | None = None
    ref: str | None = None
    edges: set[str] = field(default_factory=set)


def _node_key(
    name: str | None, version: str | None, repository: str | None
) -> tuple[str, bool, tuple[str | None, str | None]]:
    """Return ``(key, is_git, locator)`` for a library or dependency spec.

    The key is derived from the *input* spec (the registry name as written, or
    the git URL path), not the resolved canonical name. So a package referenced
    inconsistently -- bare ``name`` vs ``owner/name``, or git vs registry -- maps
    to distinct keys and isn't deduplicated; ``generate_idf_components`` warns
    about that after resolution rather than merging the nodes.
    """
    if repository:
        split_result = urlsplit(repository)
        key = str(split_result.path).strip("/").removesuffix(".git")
        ref = split_result.fragment.strip() or None
        url = urlunsplit(split_result._replace(fragment=""))
        return key, True, (url, ref)
    if name and "/" in name:
        owner, pkgname = name.split("/", 1)
    else:
        owner, pkgname = None, name
    return name, False, (owner, pkgname)


def generate_idf_components(libraries: list[Library]) -> list[IDFComponent]:
    """Resolve and convert a batch of PlatformIO libraries to IDF components.

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
    generated manifest.

    ``lib_ignore`` from ``esphome->platformio_options`` excludes libraries by
    short name (part after the ``/``), matched against both the top-level
    libraries and every dependency discovered during the graph walk.
    """
    nodes: dict[str, _LibNode] = {}

    lib_ignore = {
        name.split("/")[-1].lower()
        for name in CORE.platformio_options.get("lib_ignore", [])
    }

    # The generated CMakeLists.txt/idf_component.yml inside the shared cache
    # bake in the dependency wiring, which lib_ignore changes; salt the cache
    # path so configs with different lib_ignore values don't fight over (and
    # constantly rewrite) the same converted component files.
    salt = (
        hashlib.sha256(",".join(sorted(lib_ignore)).encode()).hexdigest()[:8]
        if lib_ignore
        else ""
    )

    def is_ignored(name: str | None) -> bool:
        if not lib_ignore or name is None:
            return False
        return name.split("/")[-1].lower() in lib_ignore

    def add_spec(name: str | None, version: str | None, repository: str | None) -> str:
        key, is_git, locator = _node_key(name, version, repository)
        node = nodes.get(key) or _LibNode(key=key, is_git=is_git)
        nodes[key] = node
        if is_git:
            node.is_git = True
            node.url, node.ref = locator
        else:
            node.owner, node.pkgname = locator
            if version:
                node.requirements.add(version)
        return key

    top_level = [
        add_spec(library.name, library.version, library.repository)
        for library in libraries
        if not is_ignored(library.name)
    ]

    # Collect + resolve to a fixpoint: a node is (re)resolved whenever its
    # requirement set has grown since the last time, so every requirement in the
    # graph is accounted for before conversion.
    components: dict[str, IDFComponent] = {}
    resolved_requirements: dict[str, frozenset[str]] = {}
    top_level_keys = set(top_level)
    worklist = deque(dict.fromkeys(top_level))
    while worklist:
        key = worklist.popleft()
        node = nodes[key]

        # A node is queued once per referring edge; skip the (uncached) registry
        # lookup + download + dependency walk unless its requirement set grew
        # since the last resolve. Requirements only ever grow, so this still
        # converges the fixpoint and terminates dependency cycles.
        requirements = frozenset(node.requirements)
        if resolved_requirements.get(key) == requirements:
            continue
        resolved_requirements[key] = requirements

        if node.is_git:
            component = IDFComponent(key, "*", GitSource(node.url, node.ref))
        else:
            owner, name, version, url = _resolve_registry_version(
                node.owner, node.pkgname, node.requirements
            )
            component = IDFComponent(
                _owner_pkgname_to_name(owner, name), version, URLSource(url)
            )
        component.download(salt=salt)

        library_json_path = component.path / "library.json"
        library_properties_path = component.path / "library.properties"
        if library_json_path.is_file():
            component.data = _parse_library_json(library_json_path)
        elif library_properties_path.is_file():
            component.data = _parse_library_properties(library_properties_path)
        else:
            raise RuntimeError(
                f"Invalid PIO library {key}: missing library.json and "
                "library.properties"
            )

        try:
            _check_library_data(component.data)
        except InvalidIDFComponent as e:
            # Skip an incompatible transitive dependency, but fail fast if a
            # top-level library the build explicitly requested is incompatible.
            if key in top_level_keys:
                raise RuntimeError(
                    f"Requested library {key} is not compatible with ESP-IDF: {e}"
                ) from e
            _LOGGER.debug("Skip incompatible dependency %s: %s", key, str(e))
            continue
        components[key] = component

        # Requirements changed (we got past the short-circuit above), so
        # (re)walk this component's dependencies.
        node.edges = set()
        for dependency in _normalize_dependencies(component.data.get("dependencies")):
            if "name" not in dependency or "version" not in dependency:
                continue
            try:
                _check_library_data(dependency)
            except InvalidIDFComponent as e:
                _LOGGER.debug("Skip dependency %s: %s", dependency.get("name"), str(e))
                continue
            dep_name = _owner_pkgname_to_name(
                dependency.get("owner"), dependency.get("name")
            )
            if is_ignored(dep_name):
                _LOGGER.debug("Skip ignored dependency %s", dep_name)
                continue
            # The version field may actually be a URL (git/archive dependency).
            dep_version = dependency["version"]
            dep_url = None
            try:
                parsed = urlparse(dep_version)
                if all([parsed.scheme, parsed.netloc]):
                    dep_url, dep_version = dep_version, None
            except (TypeError, ValueError):
                pass
            dep_key = add_spec(dep_name, dep_version, dep_url)
            node.edges.add(dep_key)
            worklist.append(dep_key)

    # A git source wins over any registry version requested for the same
    # component. That's intentional, but warn so a dropped registry pin isn't a
    # silent surprise.
    for node in nodes.values():
        if node.is_git and node.requirements:
            _LOGGER.warning(
                "Library %s is requested both from a git source (%s) and as "
                "registry version(s) %s; using the git source.",
                node.key,
                node.url,
                sorted(node.requirements),
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
    # regenerate build files.
    for key, component in components.items():
        component.dependencies = [
            components[dep_key]
            for dep_key in sorted(nodes[key].edges)
            if dep_key in components
        ]
    for component in components.values():
        _apply_extra_script(component)
        write_file_if_changed(
            component.path / "CMakeLists.txt",
            generate_cmakelists_txt(component),
        )
        write_file_if_changed(
            component.path / "idf_component.yml",
            generate_idf_component_yml(component),
        )

    return [components[key] for key in top_level if key in components]
