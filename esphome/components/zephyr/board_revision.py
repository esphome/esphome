"""Zephyr `board@revision` string parsing and `board.yml` revision resolution.

Mirrors Zephyr's own `parse_board_components()` (cmake/modules/boards.cmake) and
`board_check_revision()` (cmake/modules/extensions.cmake) so ESPHome's config-time
board resolution and DTS validation stay in lockstep with what a real `west build`
would resolve for the same `board@revision` target.
"""

from dataclasses import dataclass
from pathlib import Path
import re

import yaml

_BOARD_RE = re.compile(r"^(?P<name>[^@/]+)(@(?P<revision>[^/]+))?(?P<qualifiers>/.*)?$")


@dataclass(frozen=True)
class BoardParts:
    name: str
    revision: str | None
    qualifiers: str | None  # Includes the leading "/", or None.


def parse_board_string(board: str) -> BoardParts:
    """Split a `<board>[@<revision>][/<qualifiers>]` string into its parts."""
    m = _BOARD_RE.match(board)
    if m is None:
        # The `name` group (`[^@/]+`) matches any non-empty string not starting with
        # '@' or '/', so this should be unreachable for any non-empty board string.
        return BoardParts(board, None, None)
    return BoardParts(m.group("name"), m.group("revision"), m.group("qualifiers"))


def _revision_key(fmt: str, value: str) -> tuple[int, int, int] | int | str:
    """Return a comparable sort key for `value` in Zephyr revision format `fmt`."""
    if fmt == "number":
        return int(value)
    if fmt == "letter":
        return value
    # "major.minor.patch" -- loose typing allowed (e.g. "1" == "1.0.0"), missing
    # trailing parts default to 0.
    parts = [int(p) for p in value.split(".")]
    if len(parts) > 3:
        raise ValueError(f"Too many components in revision {value!r}")
    parts += [0] * (3 - len(parts))
    return (parts[0], parts[1], parts[2])


def _read_revision_block(board_dir: Path) -> dict | None:
    """Return `board_dir/board.yml`'s `board: revision:` block, or None if absent/unreadable."""
    board_yml = board_dir / "board.yml"
    try:
        doc = yaml.safe_load(board_yml.read_text())
    except (OSError, yaml.YAMLError):
        return None
    if not isinstance(doc, dict):
        return None
    revision = doc.get("board", {}).get("revision")
    return revision if isinstance(revision, dict) else None


def declared_revisions(board_dir: Path) -> list[str]:
    """Return the revision names `board_dir/board.yml` declares (empty if none)."""
    revision = _read_revision_block(board_dir)
    if revision is None:
        return []
    return [
        str(r["name"])
        for r in revision.get("revisions", [])
        if isinstance(r, dict) and "name" in r
    ]


def resolve_revision(board_dir: Path, requested: str) -> tuple[str | None, bool]:
    """Resolve `requested` against `board_dir/board.yml`'s `revision:` block.

    Returns `(resolved, declares_revisions)`:
    - `declares_revisions` is False when the board has no `revision:` block at all --
      nothing to validate or apply an overlay for.
    - When True, `resolved` is the matched revision string, or None when `requested`
      doesn't resolve: an exact miss under `exact: true`, or no declared revision at
      or below `requested` when `exact` is false/absent -- Zephyr's own
      `board_check_revision()` picks the closest *lower* revision in that case, never
      a higher one.
    """
    revision = _read_revision_block(board_dir)
    if revision is None:
        return None, False

    revisions = declared_revisions(board_dir)
    if not revisions:
        return None, False

    fmt = str(revision.get("format", "")).strip().lower()
    exact = bool(revision.get("exact", False))

    if requested in revisions:
        return requested, True
    if exact:
        return None, True

    try:
        requested_key = _revision_key(fmt, requested)
    except (ValueError, TypeError):
        return None, True

    best: str | None = None
    best_key = None
    for candidate in revisions:
        try:
            candidate_key = _revision_key(fmt, candidate)
        except (ValueError, TypeError):
            continue
        if candidate_key <= requested_key and (
            best_key is None or candidate_key > best_key
        ):
            best, best_key = candidate, candidate_key
    return best, True
