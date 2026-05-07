"""Config bundle creator and extractor for ESPHome.

A bundle is a self-contained .tar.gz archive containing a YAML config
and every local file it depends on. Bundles can be created from a config
and compiled directly: ``esphome compile my_device.esphomebundle.tar.gz``
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import StrEnum
import io
import json
import logging
from pathlib import Path
import re
import shutil
import tarfile
from typing import Any

from esphome import const, yaml_util
from esphome.const import (
    CONF_ESPHOME,
    CONF_EXTERNAL_COMPONENTS,
    CONF_INCLUDES,
    CONF_INCLUDES_C,
    CONF_PATH,
    CONF_SOURCE,
    CONF_TYPE,
)
from esphome.core import CORE, EsphomeError

_LOGGER = logging.getLogger(__name__)

BUNDLE_EXTENSION = ".esphomebundle.tar.gz"
MANIFEST_FILENAME = "manifest.json"
CURRENT_MANIFEST_VERSION = 1
MAX_DECOMPRESSED_SIZE = 500 * 1024 * 1024  # 500 MB
MAX_MANIFEST_SIZE = 1024 * 1024  # 1 MB

# Directories preserved across bundle extractions (build caches)
_PRESERVE_DIRS = (".esphome", ".pioenvs", ".pio")
_BUNDLE_STAGING_DIR = ".bundle_staging"


class ManifestKey(StrEnum):
    """Keys used in bundle manifest.json."""

    MANIFEST_VERSION = "manifest_version"
    ESPHOME_VERSION = "esphome_version"
    CONFIG_FILENAME = "config_filename"
    FILES = "files"
    HAS_SECRETS = "has_secrets"


# String prefixes that are never local file paths
_NON_PATH_PREFIXES = ("http://", "https://", "ftp://", "mdi:", "<")

# File extensions recognized when resolving relative path strings.
# A relative string with one of these extensions is resolved against the
# config directory and included if the file exists.
_KNOWN_FILE_EXTENSIONS = frozenset(
    {
        # Fonts
        ".ttf",
        ".otf",
        ".woff",
        ".woff2",
        ".pcf",
        ".bdf",
        # Images
        ".png",
        ".jpg",
        ".jpeg",
        ".bmp",
        ".gif",
        ".svg",
        ".ico",
        ".webp",
        # Certificates
        ".pem",
        ".crt",
        ".key",
        ".der",
        ".p12",
        ".pfx",
        # C/C++ includes
        ".h",
        ".hpp",
        ".c",
        ".cpp",
        ".ino",
        # Web assets
        ".css",
        ".js",
        ".html",
    }
)


# Matches !secret references in YAML text.  An optional surrounding
# quote pair around the key is allowed and ignored: YAML treats
# ``!secret 'foo'`` and ``!secret foo`` as the same key.  This is
# intentionally a simple regex scan rather than a YAML parse — it may
# match inside comments or multi-line strings, which is the conservative
# direction (include more secrets rather than fewer).
_SECRET_RE = re.compile(r"""!secret\s+['"]?([^\s'"]+)""")


def _find_used_secret_keys(yaml_files: list[Path]) -> set[str]:
    """Scan YAML files for ``!secret <key>`` references."""
    keys: set[str] = set()
    for fpath in yaml_files:
        try:
            text = fpath.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        for match in _SECRET_RE.finditer(text):
            keys.add(match.group(1))
    return keys


@dataclass
class BundleFile:
    """A file to include in the bundle."""

    path: str  # Relative path inside the archive
    source: Path  # Absolute path on disk


@dataclass
class BundleResult:
    """Result of creating a bundle."""

    data: bytes
    manifest: dict[str, Any]
    files: list[BundleFile]


@dataclass
class BundleManifest:
    """Parsed and validated bundle manifest."""

    manifest_version: int
    esphome_version: str
    config_filename: str
    files: list[str]
    has_secrets: bool


class ConfigBundleCreator:
    """Creates a self-contained bundle from an ESPHome config."""

    def __init__(self, config: dict[str, Any]) -> None:
        self._config = config
        self._config_dir = Path(CORE.config_dir).resolve()
        self._config_path = Path(CORE.config_path).resolve()
        self._files: list[BundleFile] = []
        self._seen_paths: set[Path] = set()
        self._secrets_paths: set[Path] = set()

    def discover_files(self) -> list[BundleFile]:
        """Discover all files needed for the bundle."""
        self._files = []
        self._seen_paths = set()
        self._secrets_paths = set()

        # The main config file
        self._add_file(self._config_path)

        # Phase 1: YAML includes (tracked during config loading)
        self._discover_yaml_includes()

        # Phase 2: Component-referenced files from validated config
        self._discover_component_files()

        return list(self._files)

    def create_bundle(self) -> BundleResult:
        """Create the bundle archive."""
        files = self.discover_files()

        # Determine which secret keys are actually referenced by the
        # bundled YAML files so we only ship those, not the entire
        # secrets.yaml which may contain secrets for other devices.
        yaml_sources = [
            bf.source for bf in files if bf.source.suffix in (".yaml", ".yml")
        ]
        used_secret_keys = _find_used_secret_keys(yaml_sources)
        filtered_secrets = self._build_filtered_secrets(used_secret_keys)

        has_secrets = bool(filtered_secrets)
        if has_secrets:
            _LOGGER.warning(
                "Bundle contains secrets (e.g. Wi-Fi passwords). "
                "Do not share it with untrusted parties."
            )

        manifest = self._build_manifest(files, has_secrets=has_secrets)

        buf = io.BytesIO()
        with tarfile.open(fileobj=buf, mode="w:gz") as tar:
            # Add manifest first
            manifest_data = json.dumps(manifest, indent=2).encode("utf-8")
            _add_bytes_to_tar(tar, MANIFEST_FILENAME, manifest_data)

            # Add filtered secrets files
            for rel_path, data in sorted(filtered_secrets.items()):
                _add_bytes_to_tar(tar, rel_path, data)

            # Add files in sorted order for determinism, skipping secrets
            # files which were already added above with filtered content
            for bf in sorted(files, key=lambda f: f.path):
                if bf.source in self._secrets_paths:
                    continue
                self._add_to_tar(tar, bf)

        return BundleResult(data=buf.getvalue(), manifest=manifest, files=files)

    def _add_file(self, abs_path: Path) -> bool:
        """Add a file to the bundle. Returns False if already added."""
        abs_path = abs_path.resolve()
        if abs_path in self._seen_paths:
            return False
        if not abs_path.is_file():
            _LOGGER.warning("Bundle: skipping missing file %s", abs_path)
            return False

        rel_path = self._relative_to_config_dir(abs_path)
        if rel_path is None:
            _LOGGER.warning(
                "Bundle: skipping file outside config directory: %s", abs_path
            )
            return False

        self._seen_paths.add(abs_path)
        self._files.append(BundleFile(path=rel_path, source=abs_path))
        return True

    def _add_directory(self, abs_path: Path) -> None:
        """Recursively add all files in a directory."""
        abs_path = abs_path.resolve()
        if not abs_path.is_dir():
            _LOGGER.warning("Bundle: skipping missing directory %s", abs_path)
            return
        for child in sorted(abs_path.rglob("*")):
            if child.is_file() and "__pycache__" not in child.parts:
                self._add_file(child)

    def _relative_to_config_dir(self, abs_path: Path) -> str | None:
        """Get a path relative to the config directory. Returns None if outside.

        Always uses forward slashes for consistency in tar archives.
        """
        try:
            return abs_path.relative_to(self._config_dir).as_posix()
        except ValueError:
            return None

    def _discover_yaml_includes(self) -> None:
        """Discover YAML files loaded during config parsing.

        Deliberately uses a fresh re-parse and force-loads every deferred
        ``IncludeFile`` to include *all* potentially-reachable includes,
        even branches not selected by the local substitutions. Bundles are
        meant to be compiled on another system where command-line
        substitution overrides may choose a different branch — e.g.
        ``!include network/${eth_model}/config.yaml`` must ship every
        candidate so the remote build can pick any one.

        Entries with unresolved substitution variables in the filename
        path are skipped with a warning (they cannot be resolved without
        the substitution pass).

        Secrets files are tracked separately so we can filter them to
        only include the keys this config actually references.
        """
        # Must be a fresh parse: IncludeFile.load() caches its result in
        # _content, and we discover files by listening for loader calls. On
        # an already-parsed tree the cache is populated, .load() returns
        # without calling the loader, the listener never fires, and the
        # referenced files would be silently dropped from the bundle.
        with yaml_util.track_yaml_loads() as loaded_files:
            try:
                data = yaml_util.load_yaml(self._config_path)
            except EsphomeError:
                _LOGGER.debug(
                    "Bundle: re-loading YAML for include discovery failed, "
                    "proceeding with partial file list"
                )
            else:
                _force_load_include_files(data)

        for fpath in loaded_files:
            if fpath == self._config_path.resolve():
                continue  # Already added as config
            if fpath.name in const.SECRETS_FILES:
                self._secrets_paths.add(fpath)
            self._add_file(fpath)

    def _discover_component_files(self) -> None:
        """Walk the validated config for file references.

        Uses a generic recursive walk to find file paths instead of
        hardcoding per-component knowledge about config dict formats.
        After validation, components typically resolve paths to absolute
        using CORE.relative_config_path() or cv.file_(). Relative paths
        with known file extensions are also resolved and checked.

        Core ESPHome concepts that use relative paths or directories
        are handled explicitly.
        """
        config = self._config

        # Generic walk: find all file paths in the validated config
        self._walk_config_for_files(config)

        # --- Core ESPHome concepts needing explicit handling ---

        # esphome.includes / includes_c - can be relative paths and directories
        esphome_conf = config.get(CONF_ESPHOME, {})
        for include_path in esphome_conf.get(CONF_INCLUDES, []):
            resolved = _resolve_include_path(include_path)
            if resolved is None:
                continue
            if resolved.is_dir():
                self._add_directory(resolved)
            else:
                self._add_file(resolved)
        for include_path in esphome_conf.get(CONF_INCLUDES_C, []):
            resolved = _resolve_include_path(include_path)
            if resolved is not None:
                self._add_file(resolved)

        # external_components with source: local - directories
        for ext_conf in config.get(CONF_EXTERNAL_COMPONENTS, []):
            source = ext_conf.get(CONF_SOURCE, {})
            if not isinstance(source, dict):
                continue
            if source.get(CONF_TYPE) != "local":
                continue
            path = source.get(CONF_PATH)
            if not path:
                continue
            p = Path(path)
            if not p.is_absolute():
                p = CORE.relative_config_path(p)
            self._add_directory(p)

    def _walk_config_for_files(self, obj: Any) -> None:
        """Recursively walk the config dict looking for file path references."""
        if isinstance(obj, dict):
            for value in obj.values():
                self._walk_config_for_files(value)
        elif isinstance(obj, (list, tuple)):
            for item in obj:
                self._walk_config_for_files(item)
        elif isinstance(obj, Path):
            if obj.is_absolute() and obj.is_file():
                self._add_file(obj)
        elif isinstance(obj, str):
            self._check_string_path(obj)

    def _check_string_path(self, value: str) -> None:
        """Check if a string value is a local file reference."""
        # Fast exits for strings that cannot be file paths
        if len(value) < 2 or "\n" in value:
            return
        if value.startswith(_NON_PATH_PREFIXES):
            return
        # File paths must contain a path separator or a dot (for extension)
        if "/" not in value and "\\" not in value and "." not in value:
            return

        p = Path(value)

        # Absolute path - check if it points to an existing file
        if p.is_absolute():
            if p.is_file():
                self._add_file(p)
            return

        # Relative path with a known file extension - likely a component
        # validator that forgot to resolve to absolute via cv.file_() or
        # CORE.relative_config_path(). Warn and try to resolve.
        if p.suffix.lower() in _KNOWN_FILE_EXTENSIONS:
            _LOGGER.warning(
                "Bundle: non-absolute path in validated config: %s "
                "(component validator should return absolute paths)",
                value,
            )
            resolved = CORE.relative_config_path(p)
            if resolved.is_file():
                self._add_file(resolved)

    def _build_filtered_secrets(self, used_keys: set[str]) -> dict[str, bytes]:
        """Build filtered secrets files containing only the referenced keys.

        Returns a dict mapping relative archive path to YAML bytes.
        """
        if not used_keys or not self._secrets_paths:
            return {}

        result: dict[str, bytes] = {}
        for secrets_path in self._secrets_paths:
            rel_path = self._relative_to_config_dir(secrets_path)
            if rel_path is None:
                continue
            try:
                all_secrets = yaml_util.load_yaml(secrets_path, clear_secrets=False)
            except EsphomeError:
                _LOGGER.warning("Bundle: failed to load secrets file %s", secrets_path)
                continue
            if not isinstance(all_secrets, dict):
                continue
            filtered = {k: v for k, v in all_secrets.items() if k in used_keys}
            if filtered:
                data = yaml_util.dump(filtered, show_secrets=True).encode("utf-8")
                result[rel_path] = data
        return result

    def _build_manifest(
        self, files: list[BundleFile], *, has_secrets: bool
    ) -> dict[str, Any]:
        """Build the manifest.json content."""
        return {
            ManifestKey.MANIFEST_VERSION: CURRENT_MANIFEST_VERSION,
            ManifestKey.ESPHOME_VERSION: const.__version__,
            ManifestKey.CONFIG_FILENAME: self._config_path.name,
            ManifestKey.FILES: [f.path for f in files],
            ManifestKey.HAS_SECRETS: has_secrets,
        }

    @staticmethod
    def _add_to_tar(tar: tarfile.TarFile, bf: BundleFile) -> None:
        """Add a BundleFile to the tar archive with deterministic metadata."""
        with open(bf.source, "rb") as f:
            _add_bytes_to_tar(tar, bf.path, f.read())


def extract_bundle(
    bundle_path: Path,
    target_dir: Path | None = None,
) -> Path:
    """Extract a bundle archive and return the path to the config YAML.

    Sanity checks reject path traversal, symlinks, absolute paths, and
    oversized archives to prevent accidental file overwrites or extraction
    outside the target directory.  These are **not** a security boundary —
    bundles are assumed to come from the user's own machine or a trusted
    build pipeline.

    Args:
        bundle_path: Path to the .tar.gz bundle file.
        target_dir: Directory to extract into. If None, extracts next to
                     the bundle file in a directory named after it.

    Returns:
        Absolute path to the extracted config YAML file.

    Raises:
        EsphomeError: If the bundle is invalid or extraction fails.
    """

    bundle_path = bundle_path.resolve()
    if not bundle_path.is_file():
        raise EsphomeError(f"Bundle file not found: {bundle_path}")

    if target_dir is None:
        target_dir = _default_target_dir(bundle_path)

    target_dir = target_dir.resolve()
    target_dir.mkdir(parents=True, exist_ok=True)

    # Read and validate the archive
    try:
        with tarfile.open(bundle_path, "r:gz") as tar:
            manifest = _read_manifest_from_tar(tar)
            _validate_tar_members(tar, target_dir)
            tar.extractall(path=target_dir, filter="data")
    except tarfile.TarError as err:
        raise EsphomeError(f"Failed to extract bundle: {err}") from err

    config_filename = manifest[ManifestKey.CONFIG_FILENAME]
    config_path = target_dir / config_filename
    if not config_path.is_file():
        raise EsphomeError(
            f"Bundle manifest references config '{config_filename}' "
            f"but it was not found in the archive"
        )

    return config_path


def read_bundle_manifest(bundle_path: Path) -> BundleManifest:
    """Read and validate the manifest from a bundle without full extraction.

    Args:
        bundle_path: Path to the .tar.gz bundle file.

    Returns:
        Parsed BundleManifest.

    Raises:
        EsphomeError: If the manifest is missing, invalid, or version unsupported.
    """

    try:
        with tarfile.open(bundle_path, "r:gz") as tar:
            manifest = _read_manifest_from_tar(tar)
    except tarfile.TarError as err:
        raise EsphomeError(f"Failed to read bundle: {err}") from err

    return BundleManifest(
        manifest_version=manifest[ManifestKey.MANIFEST_VERSION],
        esphome_version=manifest.get(ManifestKey.ESPHOME_VERSION, "unknown"),
        config_filename=manifest[ManifestKey.CONFIG_FILENAME],
        files=manifest.get(ManifestKey.FILES, []),
        has_secrets=manifest.get(ManifestKey.HAS_SECRETS, False),
    )


def _read_manifest_from_tar(tar: tarfile.TarFile) -> dict[str, Any]:
    """Read and validate manifest.json from an open tar archive."""

    try:
        member = tar.getmember(MANIFEST_FILENAME)
    except KeyError:
        raise EsphomeError("Invalid bundle: missing manifest.json") from None

    f = tar.extractfile(member)
    if f is None:
        raise EsphomeError("Invalid bundle: manifest.json is not a regular file")

    if member.size > MAX_MANIFEST_SIZE:
        raise EsphomeError(
            f"Invalid bundle: manifest.json too large "
            f"({member.size} bytes, max {MAX_MANIFEST_SIZE})"
        )

    try:
        manifest = json.loads(f.read())
    except (json.JSONDecodeError, UnicodeDecodeError) as err:
        raise EsphomeError(f"Invalid bundle: malformed manifest.json: {err}") from err

    # Version check
    version = manifest.get(ManifestKey.MANIFEST_VERSION)
    if version is None:
        raise EsphomeError("Invalid bundle: manifest.json missing 'manifest_version'")
    if not isinstance(version, int) or version < 1:
        raise EsphomeError(
            f"Invalid bundle: manifest_version must be a positive integer, got {version!r}"
        )
    if version > CURRENT_MANIFEST_VERSION:
        raise EsphomeError(
            f"Bundle manifest version {version} is newer than this ESPHome "
            f"version supports (max {CURRENT_MANIFEST_VERSION}). "
            f"Please upgrade ESPHome to compile this bundle."
        )

    # Required fields
    if ManifestKey.CONFIG_FILENAME not in manifest:
        raise EsphomeError("Invalid bundle: manifest.json missing 'config_filename'")

    return manifest


def _validate_tar_members(tar: tarfile.TarFile, target_dir: Path) -> None:
    """Sanity-check tar members to prevent mistakes and accidental overwrites.

    This is not a security boundary — bundles are created locally or come
    from a trusted build pipeline.  The checks catch malformed archives
    and common mistakes (stray absolute paths, ``..`` components) that
    could silently overwrite unrelated files.
    """

    total_size = 0
    for member in tar.getmembers():
        # Reject absolute paths (Unix and Windows)
        if member.name.startswith(("/", "\\")):
            raise EsphomeError(
                f"Invalid bundle: absolute path in archive: {member.name}"
            )

        # Reject path traversal (split on both / and \ for cross-platform)
        parts = re.split(r"[/\\]", member.name)
        if ".." in parts:
            raise EsphomeError(
                f"Invalid bundle: path traversal in archive: {member.name}"
            )

        # Reject symlinks
        if member.issym() or member.islnk():
            raise EsphomeError(f"Invalid bundle: symlink in archive: {member.name}")

        # Ensure extraction stays within target_dir
        target_path = (target_dir / member.name).resolve()
        if not target_path.is_relative_to(target_dir):
            raise EsphomeError(
                f"Invalid bundle: file would extract outside target: {member.name}"
            )

        # Track total decompressed size
        total_size += member.size
        if total_size > MAX_DECOMPRESSED_SIZE:
            raise EsphomeError(
                f"Invalid bundle: decompressed size exceeds "
                f"{MAX_DECOMPRESSED_SIZE // (1024 * 1024)}MB limit"
            )


def is_bundle_path(path: Path) -> bool:
    """Check if a path looks like a bundle file."""
    return path.name.lower().endswith(BUNDLE_EXTENSION)


def _add_bytes_to_tar(tar: tarfile.TarFile, name: str, data: bytes) -> None:
    """Add in-memory bytes to a tar archive with deterministic metadata."""
    info = tarfile.TarInfo(name=name)
    info.size = len(data)
    info.mtime = 0
    info.uid = 0
    info.gid = 0
    info.mode = 0o644
    tar.addfile(info, io.BytesIO(data))


def _force_load_include_files(obj: Any, _seen: set[int] | None = None) -> None:
    """Recursively resolve any ``IncludeFile`` instances in a YAML tree.

    Nested ``!include`` returns a deferred ``IncludeFile`` that is only
    resolved during the substitution pass. During bundle discovery we need
    the referenced files to actually load so the ``track_yaml_loads``
    listener fires for them.

    ``IncludeFile`` instances with unresolved substitution variables in the
    filename cannot be loaded — we skip and warn about those.
    """
    if _seen is None:
        _seen = set()

    if isinstance(obj, yaml_util.IncludeFile):
        if id(obj) in _seen:
            return
        _seen.add(id(obj))
        if obj.has_unresolved_expressions():
            _LOGGER.warning(
                "Bundle: cannot resolve !include %s (referenced from %s) "
                "with substitutions in path",
                obj.file,
                obj.parent_file,
            )
            return
        try:
            loaded = obj.load()
        except EsphomeError as err:
            _LOGGER.warning(
                "Bundle: failed to load !include %s (referenced from %s): %s",
                obj.file,
                obj.parent_file,
                err,
            )
            return
        _force_load_include_files(loaded, _seen)
    elif isinstance(obj, dict):
        if id(obj) in _seen:
            return
        _seen.add(id(obj))
        for value in obj.values():
            _force_load_include_files(value, _seen)
    elif isinstance(obj, (list, tuple)):
        if id(obj) in _seen:
            return
        _seen.add(id(obj))
        for item in obj:
            _force_load_include_files(item, _seen)


def _resolve_include_path(include_path: Any) -> Path | None:
    """Resolve an include path to absolute, skipping system includes."""
    if isinstance(include_path, str) and include_path.startswith("<"):
        return None  # System include, not a local file
    p = Path(include_path)
    if not p.is_absolute():
        p = CORE.relative_config_path(p)
    return p


def _default_target_dir(bundle_path: Path) -> Path:
    """Compute the default extraction directory for a bundle."""
    name = bundle_path.name
    if name.lower().endswith(BUNDLE_EXTENSION):
        name = name[: -len(BUNDLE_EXTENSION)]
    return bundle_path.parent / name


def _restore_preserved_dirs(preserved: dict[str, Path], target_dir: Path) -> None:
    """Move preserved build cache directories back into target_dir.

    If the bundle contained entries under a preserved directory name,
    the extracted copy is removed so the original cache always wins.
    """
    for dirname, src in preserved.items():
        dst = target_dir / dirname
        if dst.exists():
            shutil.rmtree(dst)
        shutil.move(str(src), str(dst))


def prepare_bundle_for_compile(
    bundle_path: Path,
    target_dir: Path | None = None,
) -> Path:
    """Extract a bundle for compilation, preserving build caches.

    Unlike extract_bundle(), this preserves .esphome/ and .pioenvs/
    directories in the target if they already exist (for incremental builds).

    Args:
        bundle_path: Path to the .tar.gz bundle file.
        target_dir: Directory to extract into. Must be specified for
                     build server use.

    Returns:
        Absolute path to the extracted config YAML file.
    """

    bundle_path = bundle_path.resolve()
    if not bundle_path.is_file():
        raise EsphomeError(f"Bundle file not found: {bundle_path}")

    if target_dir is None:
        target_dir = _default_target_dir(bundle_path)

    target_dir = target_dir.resolve()
    target_dir.mkdir(parents=True, exist_ok=True)

    preserved: dict[str, Path] = {}

    # Temporarily move preserved dirs out of the way
    staging = target_dir / _BUNDLE_STAGING_DIR
    for dirname in _PRESERVE_DIRS:
        src = target_dir / dirname
        if src.is_dir():
            dst = staging / dirname
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.move(str(src), str(dst))
            preserved[dirname] = dst

    try:
        # Clean non-preserved content and extract fresh
        for item in target_dir.iterdir():
            if item.name == _BUNDLE_STAGING_DIR:
                continue
            if item.is_dir():
                shutil.rmtree(item)
            else:
                item.unlink()

        config_path = extract_bundle(bundle_path, target_dir)
    finally:
        # Restore preserved dirs (idempotent) and clean staging
        _restore_preserved_dirs(preserved, target_dir)
        if staging.is_dir():
            shutil.rmtree(staging)

    return config_path
