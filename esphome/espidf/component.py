from collections.abc import Callable
import glob
import hashlib
import itertools
import json
import logging
import os
from pathlib import Path
import re
import tempfile
from typing import TypeVar
from urllib.parse import urlparse, urlsplit, urlunsplit

from esphome import git, yaml_util
from esphome.const import KEY_CORE, KEY_FRAMEWORK_VERSION
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

#
# Constants for workarounds
#

REQUIRES_DETECT_PATTERNS = {
    "mbedtls": [re.compile(r'^\s*#\s*include\s*[<"]mbedtls[^">]*[">]', re.MULTILINE)],
    "esp_netif": [
        re.compile(r'^\s*#\s*include\s*[<"]esp_netif[^">]*[">]', re.MULTILINE)
    ],
    "esp_driver_gpio": [
        re.compile(r'^\s*#\s*include\s*[<"]driver/gpio\.h[^">]*[">]', re.MULTILINE)
    ],
    "esp_timer": [
        re.compile(r'^\s*#\s*include\s*[<"]esp_timer\.h[^">]*[">]', re.MULTILINE)
    ],
    "esp_wifi": [
        re.compile(
            r'^\s*#\s*include\s*[<"]WiFi\.h[^">]*[">]', re.MULTILINE
        )  # Arduino WiFi
    ],
}

ESPHOME_DATA_KEY = "ESPHOME"
ESPHOME_DATA_EXTRA_CMAKE_KEY = "EXTRA_CMAKE"


class Source:
    def download(self, dir_suffix: str, force: bool = False) -> Path:
        raise NotImplementedError()


class URLSource(Source):
    def __init__(self, url: str):
        self.url = url

    def download(self, dir_suffix: str, force: bool = False) -> Path:
        base_dir = Path(CORE.data_dir) / DOMAIN
        h = hashlib.new("sha256")
        h.update(self.url.encode())
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
    def __init__(self, url: str, ref: str):
        self.url = url
        self.ref = ref

    def download(self, dir_suffix: str, force: bool = False) -> Path:
        path, _ = git.clone_or_update(
            url=self.url,
            ref=self.ref,
            refresh=git.NEVER_REFRESH if not force else None,
            domain=DOMAIN,
            submodules=[],
            subpath=Path(dir_suffix),
        )
        return path

    def __str__(self):
        return f"{self.url}#{self.ref}"


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

    def download(self, force: bool = False):
        """
        The dependency name should match the directory name at the end of the override path.
        The ESP-IDF build system uses the directory name as the component name, so the directory of the override_path should match the component name.
        If you want to specify the full name of the component with the namespace, replace / in the component name with __.
        @see https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html
        """
        self.path = self.source.download(self.get_sanitized_name(), force=force)


def _sanitize_version(version: str) -> str:
    """
    Sanitize a version string by removing common requirement prefixes or a leading v.

    Args:
        version: Version string to clean.

    Returns:
        Cleaned version string without common requirement symbols.
    """
    version = version.strip()

    prefixes = (
        "^",
        "~=",
        "~",
        ">=",
        "<=",
        "==",
        "!=",
        ">",
        "<",
        "=",
        "v",
        "V",
    )

    for p in prefixes:
        if version.startswith(p):
            version = version[len(p) :]
            break

    return version.strip()


def _get_package_from_pio_registry(
    username: str | None, pkgname: str, requirements: str
) -> tuple[str, str, str | None, str | None]:
    """
    Fetch package information from PlatformIO registry.

    This function queries the PlatformIO registry to find a library package
    that matches the given criteria and returns its metadata including version
    and download URL.

    Args:
        username: The owner/username of the package (can be None)
        pkgname: The name of the package
        requirements: Version requirements (e.g., "^1.0.0")

    Returns:
        tuple[str, str, str | None, str | None]:
        A tuple containing (owner, name, version, download_url)
        where version and download_url can be None if not found
    """

    from platformio.package.manager._registry import PackageManagerRegistryMixin
    from platformio.package.meta import PackageSpec

    # Create a minimal PackageManagerRegistry class
    class PackageManagerRegistry(PackageManagerRegistryMixin):
        def __init__(self):
            self._registry_client = None
            self.pkg_type = "library"

        @staticmethod
        def is_system_compatible(value, custom_system=None):
            return True

    pio_registry = PackageManagerRegistry()

    # Fetch package metadata from registry
    package = pio_registry.fetch_registry_package(
        PackageSpec(
            owner=username,
            name=pkgname,
        )
    )
    owner = package["owner"]["username"]
    name = package["name"]

    # Find the best matching version based on requirements
    version = pio_registry.pick_best_registry_version(
        package.get("versions"),
        PackageSpec(owner=username, name=pkgname, requirements=requirements),
    )

    #  If no version found, return with None for version and URL
    if not version:
        return owner, name, None, None

    # Find the compatible package file for this version
    pkgfile = pio_registry.pick_compatible_pkg_file(version["files"])

    #  If no package file found, return with None for URL but valid version
    if not pkgfile:
        return owner, name, version["name"], None

    return owner, name, version["name"], pkgfile["download_url"]


def _patch_component(component: IDFComponent, first_pass: bool):
    """
    Apply patches/workarounds to specific components that have known issues.

    This function modifies component data to fix compatibility issues or missing
    dependencies for certain libraries. It applies different patches based on
    whether it's the first or second pass of processing.

    Args:
        component: The IDFComponent object to potentially patch
        first_pass: Boolean indicating if this is the first pass of processing
    """

    # Patch only on the second step
    if not first_pass and CORE.using_arduino:
        # Add the missing dependency to Arduino framework. Source is None so
        # the IDF component manager resolves it from the registry instead of
        # cloning the 2 GB arduino-esp32 git history.
        component.dependencies.append(
            IDFComponent(
                "espressif/arduino-esp32",
                str(CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]),
                None,
            )
        )

    #
    # fastled/FastLED
    #

    # Patch only on the first step
    if (
        first_pass
        and component.name == _owner_pkgname_to_name("fastled", "FastLED")
        and not (component.path / "idf_component.yml").is_file()
    ):
        # Force fake idf_component: This project already support ESP-IDF
        (component.path / "idf_component.yml").write_text("")


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

        full_pattern = os.path.join(glob.escape(str(src_dir)), pattern)

        matched = []
        for item in glob.glob(full_pattern, recursive=True):
            if not os.path.isdir(item):
                matched.append(item)
            else:
                # PlatformIO quirk: a directory matched with "*" should include all its
                # nested files and subdirectories, not just the directory itself.
                for root, _, files in os.walk(item):
                    matched.extend([os.path.join(root, f) for f in files])

        if sign == "+":
            selected.update(matched)
        elif sign == "-":
            selected.difference_update(matched)

    return [r for r in selected if os.path.isfile(r)]


def _convert_library_to_component(library: Library) -> IDFComponent:
    """
    Convert a Library object to an IDFComponent object by resolving its metadata.

    This function handles the conversion of library specifications to component
    objects, resolving versions through PlatformIO registry when needed or
    parsing direct repository URLs.

    Args:
        library: The Library object containing name, version, and/or repository information

    Returns:
        IDFComponent: The resolved component with name, version, and URL

    Raises:
        ValueError: If a repository URL is missing a reference (#)
        RuntimeError: If no artifact can be found for the library
    """
    name = None
    version = None
    source = None

    #  Repository is provided directly
    if library.repository:
        # Parse repository URL to extract name and version
        split_result = urlsplit(library.repository)
        if not split_result.fragment.strip():
            raise ValueError(f"Missing ref in URL {library.repository}")

        # Sanitize name
        name = str(split_result.path).strip("/")
        name = name.removesuffix(".git")

        # Sanitize version
        version = _sanitize_version(split_result.fragment)
        repository = urlunsplit(split_result._replace(fragment=""))

        source = GitSource(str(repository), split_result.fragment)

    # Version is provided - resolve using PlatformIO registry
    elif library.version:
        name = library.name
        if "/" not in name:
            owner, pkgname = None, name
        else:
            owner, pkgname = name.split("/", 1)

        owner, pkgname, version, url = _get_package_from_pio_registry(
            owner, pkgname, library.version
        )
        if url is None:
            raise RuntimeError(
                f"Can't find an pkg file from PlatformIO registry for library {library}"
            )

        name = _owner_pkgname_to_name(owner, pkgname)
        source = URLSource(url)

    if source is None:
        raise RuntimeError(f"Can't find an artifact associated to library {library}")

    assert name, "Missing library name"
    assert version, "Missing library version"

    return IDFComponent(name, version, source)


def _detect_requires(build_src_files: list[str]) -> set[str]:
    """
    Detect required components from source files.

    Args:
        build_src_files: List of source file paths to analyze

    Returns:
        Set of detected required components
    """
    detected = set()

    # 1. Process each source file
    for file in build_src_files:
        path = Path(file)

        if not path.is_file():
            continue

        try:
            content = path.read_text(encoding="utf-8", errors="ignore")
        except Exception:  # pylint: disable=broad-exception-caught
            continue

        # 2. Add required component if one of these patterns matches
        for require_name, patterns in REQUIRES_DETECT_PATTERNS.items():
            if require_name in detected:
                continue  # already found

            for pattern in patterns:
                if pattern.search(content):
                    detected.add(require_name)
                    break

    return detected


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

    # Detect in the files which requirements to add
    # By default in platformio, all the components are added: we need to detect them when using ESP-IDF
    requires = _detect_requires(build_src_files)

    # Dependencies are required
    for dependency in component.dependencies:
        requires.add(dependency.get_require_name())

    # Only keep sources
    build_src_files = [os.path.relpath(p, component.path) for p in build_src_files]
    build_src_files = [
        f for f in build_src_files if os.path.splitext(f)[1] in SRC_FILE_EXTENSIONS
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
    if requires:
        str_requires = " ".join(sorted(requires))
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

    # Do not use the version from library.json/library.properties; it may be incorrect.
    data["version"] = component.version

    repository = component.data.get("repository", {}).get("url", None)
    if repository:
        data["repository"] = repository

    for dependency in component.dependencies:
        # Initialize dependencies section if needed
        if "dependencies" not in data:
            data["dependencies"] = {}

        # Add this dependency to dependencies
        dep = {}
        dep["version"] = dependency.version

        # Should use dependency.path as override path
        try:
            dep["override_path"] = str(dependency.path)
        except RuntimeError as e:
            # No local path; let the IDF component manager resolve.
            # GitSource gives an explicit URL; arduino-esp32 is resolved by
            # version from the registry. Anything else is a bug.
            if isinstance(dependency.source, GitSource):
                dep["git"] = dependency.source.url
            elif dependency.name != "espressif/arduino-esp32":
                raise e

        data["dependencies"][dependency.get_sanitized_name()] = dep

    return yaml_util.dump(data)


def _check_library_data(data: dict):
    """
    Check if a library data is compatible with the ESP-IDF framework.

    Args:
        component: IDFComponent object being processed

    Raises:
        ValueError: If library has unsupported platforms or frameworks
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

    # Check if library supports ESP-IDF framework
    framework = "arduino" if CORE.using_arduino else "espidf"
    valid_framework = "*" in frameworks or framework in frameworks

    if not valid_framework:
        raise InvalidIDFComponent(f"Unsupported library frameworks: {frameworks}")

    extra_script = data.get("build", {}).get("extraScript", None)
    if extra_script:
        _LOGGER.warning(
            'Extra scripts are not supported. The script "%s" will not be executed.',
            extra_script,
        )


def _process_dependencies(component: IDFComponent):
    """
    Process library dependencies and generate ESP-IDF components.

    Args:
        component: IDFComponent object being processed

    Returns:
        None
    """

    name, version = component.name, component.version
    dependencies = component.data.get("dependencies")
    if not dependencies:
        return

    _LOGGER.info("Processing %s@%s component dependencies...", name, version)
    for dependency in dependencies:
        # Validate dependency structure
        if not all(k in dependency for k in ("name", "version")):
            _LOGGER.debug("Ignore invalid library: %s", dependency)
            continue

        try:
            _check_library_data(dependency)
        except InvalidIDFComponent as e:
            _LOGGER.debug(
                "Skip %s@%s: %s", dependency["name"], dependency["version"], str(e)
            )
            continue

        # The version field may actually contain a URL
        version = dependency["version"]
        url = None
        try:
            result = urlparse(version)
            if all([result.scheme, result.netloc]):
                url, version = version, None
        except (TypeError, ValueError):
            pass

        # Generate ESP-IDF component from PlatformIO library
        component.dependencies.append(
            _generate_idf_component(
                Library(
                    _owner_pkgname_to_name(
                        dependency.get("owner", None), dependency.get("name")
                    ),
                    version,
                    url,
                )
            )
        )


def _parse_library_json(library_json_path: PathType):
    """
    Load and parse a JSON file describing a library.

    Args:
        library_json_path (PathType): Path to the JSON file.

    Returns:
        dict: Parsed JSON content as a Python dictionary.
    """
    with open(library_json_path, encoding="utf8") as fp:
        return json.load(fp)


def _parse_library_properties(library_properties_path: PathType):
    """
    Parse a key-value platformio .properties style file into a dictionary.

    Args:
        library_properties_path (PathType): Path to the properties file.

    Returns:
        dict[str, str]: Mapping of parsed property keys to values.
    """
    with open(library_properties_path, encoding="utf8") as fp:
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


def _generate_idf_component(library: Library, force: bool = False) -> IDFComponent:
    """
    Generate an ESP-IDF component from a library specification.

    This function resolves the library, downloads it, processes metadata files,
    and generates necessary ESP-IDF build files (CMakeLists.txt, idf_component.yml).

    Args:
        library: The library specification containing name, version, and repository URL
        force: If True, forces re-download of the library even if it exists locally

    Returns:
        IDFComponent: The generated component object with resolved metadata
    """
    _LOGGER.info("Generate IDF component for %s library ...", library)

    # Resolve component name, version and url
    component = _convert_library_to_component(library)
    name, version = component.name, component.version

    # Download the library
    component.download(force)

    # Paths to component metadata and build files
    library_json_path = component.path / "library.json"
    library_properties_path = component.path / "library.properties"
    cmakelists_txt_path = component.path / "CMakeLists.txt"
    idf_component_yml_path = component.path / "idf_component.yml"

    # Apply patches to the library metadata
    _patch_component(component, True)

    if cmakelists_txt_path.is_file() and idf_component_yml_path.is_file():
        # Already an ESP-IDF component
        return component

    if library_json_path.is_file():
        component.data = _parse_library_json(library_json_path)
    elif library_properties_path.is_file():
        component.data = _parse_library_properties(library_properties_path)
    else:
        raise RuntimeError(
            "Invalid PIO library: missing library.json and/or library.properties"
        )

    # Apply additional patches to the library metadata
    _patch_component(component, False)

    # Check if the component is usable with ESP-IDF
    _check_library_data(component.data)

    # Handle the dependencies (convert PlatformIO library to ESP-IDF component if needed)
    _process_dependencies(component)

    # Generate files
    _LOGGER.debug("Generating CMakeLists.txt for %s@%s  ...", name, version)
    write_file_if_changed(
        cmakelists_txt_path,
        generate_cmakelists_txt(component),
    )

    _LOGGER.debug("Generating idf_component.yml for %s@%s  ...", name, version)
    write_file_if_changed(
        idf_component_yml_path,
        generate_idf_component_yml(component),
    )

    return component


def generate_idf_component(
    library: Library, force: bool = False
) -> tuple[str, str, Path]:
    """
    Generate an ESP-IDF component and return its name, version, and path.

    This is a wrapper function that calls _generate_idf_component and returns
    the standardized tuple format (name, version, path).

    Args:
        library: The library specification containing name, version, and repository URL
        force: If True, forces re-download of the library even if it exists locally

    Returns:
        tuple[str, str, Path]: A tuple containing (component_name, component_version, component_path)
    """
    component = _generate_idf_component(library, force)
    return component.get_sanitized_name(), component.version, component.path
