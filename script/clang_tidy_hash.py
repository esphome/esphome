"""Files that affect clang-tidy results and the idedata built from them.

``CLANG_TIDY_GLOBAL_FILES`` (plus ``SDKCONFIG_DEFAULTS_PREFIX``) lists the files
that influence clang-tidy output; ``script/determine-jobs.py`` runs a full scan
when one changes. ``ESP_IDF_INFRA_TRIGGER_*`` lists the native ESP-IDF build
code. ``idedata_cache_hash()`` folds the right set into the idedata cache key
used by ``script/helpers.py`` and the CI cache action.
"""

from __future__ import annotations

import hashlib
from pathlib import Path

# Root-relative paths whose contents affect clang-tidy results.
CLANG_TIDY_GLOBAL_FILES = (
    ".clang-tidy",
    "script/clang-tidy",
    "platformio.ini",
    "requirements_dev.txt",
    "esphome/idf_component.yml",
    "esphome/components/esp32/__init__.py",
    "esphome/components/nrf52/__init__.py",
)

# sdkconfig.defaults and per-target sdkconfig.defaults.<target> files flip the
# CONFIG flags that decide which variant code paths clang-tidy sees. Matched by
# this prefix at the repo root.
SDKCONFIG_DEFAULTS_PREFIX = "sdkconfig.defaults"

# Native ESP-IDF build infra: determine-jobs forces an esp32 compile when these
# change, and they feed the clang-tidy idedata cache key.
ESP_IDF_INFRA_TRIGGER_PATH_PREFIXES = ("esphome/espidf/", "esphome/build_helpers/")
ESP_IDF_INFRA_TRIGGER_FILES = frozenset(
    {
        "esphome/build_gen/espidf.py",
        "esphome/framework_helpers.py",
        "esphome/platformio/library.py",
        "esphome/platformio/extra_script.py",
    }
)


def read_file_bytes(path: Path) -> bytes:
    """Read bytes from a file."""
    with path.open("rb") as f:
        return f.read()


def get_repo_root() -> Path:
    """Get the repository root directory."""
    return Path(__file__).parent.parent


def _ensure_repo_root(repo_root: Path | None) -> Path:
    """Ensure repo_root is a Path, using default if None."""
    return repo_root if repo_root is not None else get_repo_root()


def calculate_clang_tidy_hash(repo_root: Path | None = None) -> str:
    """Calculate a hash of the files that affect clang-tidy results."""
    repo_root = _ensure_repo_root(repo_root)

    hasher = hashlib.sha256()

    for name in CLANG_TIDY_GLOBAL_FILES:
        path = repo_root / name
        if path.exists():
            hasher.update(read_file_bytes(path))

    # Hash each sdkconfig.defaults* file. Include the filename so adding or
    # renaming a per-target variant is detected, not just content edits.
    for path in sorted(repo_root.glob(f"{SDKCONFIG_DEFAULTS_PREFIX}*")):
        hasher.update(path.name.encode())
        hasher.update(read_file_bytes(path))

    return hasher.hexdigest()


def calculate_idedata_cache_hash(repo_root: Path | None = None) -> str:
    """Clang-tidy hash plus the Python that generates the idedata."""
    repo_root = _ensure_repo_root(repo_root)

    hasher = hashlib.sha256()
    hasher.update(calculate_clang_tidy_hash(repo_root).encode())

    paths = {repo_root / name for name in ESP_IDF_INFRA_TRIGGER_FILES}
    for prefix in ESP_IDF_INFRA_TRIGGER_PATH_PREFIXES:
        # .pyc files appear between the CI key computation and load_idedata's.
        paths.update(
            path
            for path in (repo_root / prefix).rglob("*")
            if "__pycache__" not in path.parts
        )
    for path in sorted(paths):
        if path.is_file():
            hasher.update(str(path.relative_to(repo_root)).encode())
            hasher.update(read_file_bytes(path))

    return hasher.hexdigest()


def idedata_cache_hash(environment: str, repo_root: Path | None = None) -> str:
    """Hash gating the cached idedata of one clang-tidy environment."""
    if "esp32" in environment:
        return calculate_idedata_cache_hash(repo_root)
    return calculate_clang_tidy_hash(repo_root)
