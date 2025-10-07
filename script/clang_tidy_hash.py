#!/usr/bin/env python3
"""Calculate and manage hash for clang-tidy configuration."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import sys

# Add the script directory to path to import helpers
script_dir = Path(__file__).parent
sys.path.insert(0, str(script_dir))


def read_file_lines(path: Path) -> list[str]:
    """Read lines from a file."""
    with open(path) as f:
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
    with open(path, "rb") as f:
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

    # Hash sdkconfig.defaults file
    sdkconfig_path = repo_root / "sdkconfig.defaults"
    if sdkconfig_path.exists():
        sdkconfig_content = read_file_bytes(sdkconfig_path)
        hasher.update(sdkconfig_content)

    return hasher.hexdigest()


def read_stored_hash(repo_root: Path | None = None) -> str | None:
    """Read the stored hash from file"""
    repo_root = _ensure_repo_root(repo_root)
    hash_file = repo_root / ".clang-tidy.hash"
    if hash_file.exists():
        lines = read_file_lines(hash_file)
        return lines[0].strip() if lines else None
    return None


def write_file_content(path: Path, content: str) -> None:
    """Write content to a file."""
    with open(path, "w") as f:
        f.write(content)


def write_hash(hash_value: str, repo_root: Path | None = None) -> None:
    """Write hash to file"""
    repo_root = _ensure_repo_root(repo_root)
    hash_file = repo_root / ".clang-tidy.hash"
    # Strip any trailing newlines to ensure consistent formatting
    write_file_content(hash_file, hash_value.strip() + "\n")


def main() -> None:
    parser = argparse.ArgumentParser(description="Manage clang-tidy configuration hash")
    parser.add_argument(
        "--check",
        action="store_true",
        help="Check if full scan needed (exit 0 if needed)",
    )
    parser.add_argument("--update", action="store_true", help="Update the hash file")
    parser.add_argument(
        "--update-if-changed",
        action="store_true",
        help="Update hash only if configuration changed (for pre-commit)",
    )
    parser.add_argument(
        "--verify", action="store_true", help="Verify hash matches (for CI)"
    )

    args = parser.parse_args()

    current_hash = calculate_clang_tidy_hash()
    stored_hash = read_stored_hash()

    if args.check:
        # Check if hash changed OR if .clang-tidy.hash was updated in this PR
        # This is used in CI to determine if a full clang-tidy scan is needed
        hash_changed = current_hash != stored_hash

        # Lazy import to avoid requiring dependencies that aren't needed for other modes
        from helpers import changed_files  # noqa: E402

        hash_file_updated = ".clang-tidy.hash" in changed_files()

        # Exit 0 if full scan needed
        sys.exit(0 if (hash_changed or hash_file_updated) else 1)

    elif args.verify:
        # Verify that hash file is up to date with current configuration
        # This is used in pre-commit and CI checks to ensure hash was updated
        if current_hash != stored_hash:
            print("ERROR: Clang-tidy configuration has changed but hash not updated!")
            print(f"Expected: {current_hash}")
            print(f"Found: {stored_hash}")
            print("\nPlease run: script/clang_tidy_hash.py --update")
            sys.exit(1)
        print("Hash verification passed")

    elif args.update:
        write_hash(current_hash)
        print(f"Hash updated: {current_hash}")

    elif args.update_if_changed:
        if current_hash != stored_hash:
            write_hash(current_hash)
            print(f"Clang-tidy hash updated: {current_hash}")
            # Exit 0 so pre-commit can stage the file
            sys.exit(0)
        else:
            print("Clang-tidy hash unchanged")
            sys.exit(0)

    else:
        print(f"Current hash: {current_hash}")
        print(f"Stored hash: {stored_hash}")
        print(f"Match: {current_hash == stored_hash}")


if __name__ == "__main__":
    main()
