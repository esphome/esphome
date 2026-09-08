#!/usr/bin/env python3
"""Keep pre-commit hook revs in sync with the requirements files.

Dependabot only bumps the ``package==version`` pins in ``requirements*.txt``.
Some of those tools are pinned a second time as hook ``rev`` values in
``.pre-commit-config.yaml``. This script treats the requirements files as
the source of truth and rewrites the revs to match, editing the config
through yamlrocks so comments and layout survive.

Run without arguments to apply the changes in place, or with ``--check`` to
only report drift (exit status 1 when anything is out of sync).
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import sys
from typing import Any

import yamlrocks

REPO_ROOT = Path(__file__).resolve().parent.parent
PRECOMMIT_CONFIG = ".pre-commit-config.yaml"


class SyncError(Exception):
    """A pin could not be located in a requirements file or the config."""


@dataclass(frozen=True)
class SyncTarget:
    """A requirements pin and the pre-commit repo whose rev mirrors it."""

    package: str
    requirements_file: str
    repo: str


SYNC_TARGETS: tuple[SyncTarget, ...] = (
    SyncTarget(
        "ruff", "requirements_test.txt", "https://github.com/astral-sh/ruff-pre-commit"
    ),
    SyncTarget("flake8", "requirements_test.txt", "https://github.com/PyCQA/flake8"),
    SyncTarget(
        "pyupgrade", "requirements_test.txt", "https://github.com/asottile/pyupgrade"
    ),
    SyncTarget(
        "clang-format",
        "requirements_dev.txt",
        "https://github.com/pre-commit/mirrors-clang-format",
    ),
    SyncTarget(
        "yamllint",
        "requirements_dev.txt",
        "https://github.com/adrienverge/yamllint.git",
    ),
)


def read_requirement_version(requirements: str, package: str) -> str | None:
    """Return the ``==`` pin for ``package`` or None when it is not pinned."""
    pattern = re.compile(
        rf"^{re.escape(package)}==(?P<version>[^\s#]+)",
        re.MULTILINE | re.IGNORECASE,
    )
    match = pattern.search(requirements)
    return match.group("version") if match else None


def find_repo_entry(doc: Any, repo: str) -> Any:
    """Return the single ``- repo:`` block for ``repo`` in a pre-commit doc."""
    try:
        entries = [entry for entry in doc["repos"] if entry["repo"] == repo]
    except KeyError as err:
        raise SyncError(f"malformed pre-commit config, missing key {err}") from None
    if len(entries) != 1:
        raise SyncError(
            f"expected exactly one block for repo {repo}, found {len(entries)}"
        )
    return entries[0]


def current_rev(entry: Any, repo: str) -> tuple[str, str]:
    """Split the block's rev into its tag prefix (``v`` or empty) and version."""
    if "rev" not in entry:
        raise SyncError(f"repo {repo} has no rev")
    rev = entry["rev"]
    if not isinstance(rev, str):
        # A rev such as ``1.0`` parses as a number and cannot be compared or
        # rewritten safely; quote it in the config instead.
        raise SyncError(f"rev of repo {repo} is not a string: {rev!r}")
    prefix = "v" if rev.startswith("v") else ""
    return prefix, rev.removeprefix("v")


def sync(root: Path, *, write: bool) -> list[str]:
    """Bring every hook rev in line with its requirements pin.

    Returns one description per rev that was (or, when ``write`` is False,
    would be) changed. Raises SyncError when a pin cannot be found, which
    means SYNC_TARGETS has gone stale and needs updating by hand.
    """
    config_path = root / PRECOMMIT_CONFIG
    doc = yamlrocks.loads(config_path.read_bytes(), option=yamlrocks.OPT_ROUND_TRIP)
    requirements: dict[str, str] = {}
    changes: list[str] = []
    for target in SYNC_TARGETS:
        if target.requirements_file not in requirements:
            requirements[target.requirements_file] = (
                root / target.requirements_file
            ).read_text()
        version = read_requirement_version(
            requirements[target.requirements_file], target.package
        )
        if version is None:
            raise SyncError(
                f"{target.requirements_file}: no '{target.package}==' pin found"
            )

        entry = find_repo_entry(doc, target.repo)
        prefix, current = current_rev(entry, target.repo)
        if current == version:
            continue
        changes.append(f"{target.package}: {current} -> {version}")
        entry["rev"] = f"{prefix}{version}"

    if changes and write:
        config_path.write_bytes(doc.to_yaml())
    return changes


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--check",
        action="store_true",
        help="report drift without modifying any file; exit 1 if out of sync",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=REPO_ROOT,
        help="repository checkout to operate on (default: this checkout)",
    )
    args = parser.parse_args(argv)

    try:
        changes = sync(args.root, write=not args.check)
    except SyncError as err:
        print(f"error: {err}", file=sys.stderr)
        return 1

    for change in changes:
        print(change)
    if args.check and changes:
        return 1
    return 0


if __name__ == "__main__":  # pragma: no cover
    sys.exit(main())
