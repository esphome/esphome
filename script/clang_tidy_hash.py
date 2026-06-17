"""Calculate a content hash of the clang-tidy build inputs.

Used by ``script/helpers.py`` as a cache key for generated idedata: the hash
covers every file baked into idedata (``.clang-tidy``, ``platformio.ini``, the
clang-tidy version, ``sdkconfig.defaults*`` and ``esphome/idf_component.yml``),
so the cache can't drift from that file list.
"""

from __future__ import annotations

import hashlib
from pathlib import Path
import re


def read_file_lines(path: Path) -> list[str]:
    """Read lines from a file."""
    with path.open() as f:
        return f.readlines()


def parse_requirement_line(line: str) -> tuple[str, str] | None:
    """Parse a requirement line and return (package, original_line) or None.

    Handles formats like:
    - package==1.2.3
    - package==1.2.3  # comment
    - package>=1.2.3,<2.0.0
    """
    original_line = line.strip()

    # Extract the part before any comment for parsing
    parse_line = line
    if "#" in parse_line:
        parse_line = parse_line[: parse_line.index("#")]

    parse_line = parse_line.strip()
    if not parse_line:
        return None

    # Use regex to extract package name
    # This matches package names followed by version operators
    match = re.match(r"^([a-zA-Z0-9_-]+)(==|>=|<=|>|<|!=|~=)(.+)$", parse_line)
    if match:
        return (match.group(1), original_line)  # Return package name and original line

    return None


def get_clang_tidy_version_from_requirements(repo_root: Path | None = None) -> str:
    """Get clang-tidy version from requirements_dev.txt"""
    repo_root = _ensure_repo_root(repo_root)
    requirements_path = repo_root / "requirements_dev.txt"
    lines = read_file_lines(requirements_path)

    for line in lines:
        parsed = parse_requirement_line(line)
        if parsed and parsed[0] == "clang-tidy":
            # Return the original line (preserves comments)
            return parsed[1]

    return "clang-tidy version not found"


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
    """Calculate hash of clang-tidy configuration and version"""
    repo_root = _ensure_repo_root(repo_root)

    hasher = hashlib.sha256()

    # Hash .clang-tidy file
    clang_tidy_path = repo_root / ".clang-tidy"
    content = read_file_bytes(clang_tidy_path)
    hasher.update(content)

    # Hash clang-tidy version from requirements_dev.txt
    version = get_clang_tidy_version_from_requirements(repo_root)
    hasher.update(version.encode())

    # Hash the entire platformio.ini file
    platformio_path = repo_root / "platformio.ini"
    platformio_content = read_file_bytes(platformio_path)
    hasher.update(platformio_content)

    # Hash sdkconfig.defaults and any per-target sdkconfig.defaults.<target>:
    # the per-target files flip CONFIG flags that change which variant code
    # paths clang-tidy sees. Include the filename so a rename is detected.
    for sdkconfig_path in sorted(repo_root.glob("sdkconfig.defaults*")):
        hasher.update(sdkconfig_path.name.encode())
        hasher.update(read_file_bytes(sdkconfig_path))

    # Hash esphome/idf_component.yml: its managed deps drive the ESP-IDF
    # build's include set, which clang-tidy analyzes.
    idf_component_path = repo_root / "esphome" / "idf_component.yml"
    if idf_component_path.exists():
        hasher.update(read_file_bytes(idf_component_path))

    return hasher.hexdigest()
