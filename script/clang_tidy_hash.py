"""Files that affect clang-tidy results, and a content hash over them.

``CLANG_TIDY_GLOBAL_FILES`` (plus ``SDKCONFIG_DEFAULTS_PREFIX``) is the single
source of truth for which files influence clang-tidy output. A change to any of
them can surface warnings in source files a PR didn't touch, so:

* ``script/determine-jobs.py`` runs a full clang-tidy scan when one changes, and
* ``calculate_clang_tidy_hash()`` folds them into the idedata cache key used by
  ``script/helpers.py`` (a content hash, unlike an mtime check, stays correct
  across git checkouts).
"""

from __future__ import annotations

import hashlib
from pathlib import Path

# Root-relative paths whose contents affect clang-tidy results.
CLANG_TIDY_GLOBAL_FILES = (
    ".clang-tidy",
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
