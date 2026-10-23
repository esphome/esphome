"""Operator manifest helpers for the tflite_op_processor component.

Parses the X-macro operator table (``tflm_operators.h``) so the set of
supported TFLite Micro operators is available to the ESPHome code generator at
compile (config) time, and reads the operator list of a model from its
companion ``.txt`` metadata report. The C++ table remains the single source of
truth; this module only reads it and never edits it.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re

# X-macro entry patterns used in tflm_operators.h. Inline comments are stripped
# before matching, so a trailing "// note" is allowed.
_AVAILABLE_RE = re.compile(r"^TFLM_OP_AVAILABLE\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)$")
_CONDITIONAL_RE = re.compile(
    r"^TFLM_OP_CONDITIONAL\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)$"
)
_UNAVAILABLE_RE = re.compile(r"^TFLM_OP_UNAVAILABLE\(([A-Za-z0-9_]+)\)$")
# A line in the model .txt report listing one operator type, e.g. "  REDUCE_MIN: 1"
_OP_COUNT_RE = re.compile(r"^\s*([A-Z][A-Z0-9_]*):\s+\d+\s*$")

_TABLE_START = "#ifdef TFLM_OPERATORS_ACTIVE"
_TABLE_END = "#endif"


@dataclass(frozen=True)
class OpTable:
    """Operator table split by how the resolver registers each operator."""

    available: frozenset[str]
    conditional: frozenset[str]
    unavailable: frozenset[str]

    @property
    def supported(self) -> frozenset[str]:
        """Operators the pinned esp-tflite-micro can register."""
        return self.available | self.conditional


def table_path() -> Path:
    """Return the absolute path of the operator table shipped with this module."""
    return Path(__file__).resolve().parent / "tflm_operators.h"


def parse_op_table(path: str | Path | None = None) -> OpTable:
    """Parse tflm_operators.h into the operator sets used by the resolver.

    Args:
        path: optional explicit path to the table (defaults to the table in the
            same directory as this module).

    Returns:
        An :class:`OpTable` with the unconditional, conditional and unavailable
        operator sets. A model may use ``table.supported``.

    Raises:
        ValueError: if the table has no ``TFLM_OPERATORS_ACTIVE`` block, or a
            line in the block is neither a comment nor one of the three entry
            kinds, so the C++ table and this parser cannot drift apart.
    """
    table = Path(path) if path is not None else table_path()
    available: set[str] = set()
    conditional: set[str] = set()
    unavailable: set[str] = set()

    in_table = False
    seen_table = False
    for lineno, raw_line in enumerate(
        table.read_text(encoding="utf-8").splitlines(), start=1
    ):
        line = raw_line.split("//", 1)[0].strip()
        if not line:
            continue
        if line == _TABLE_START:
            in_table = True
            seen_table = True
            continue
        if not in_table:
            # File header (pragma, includes, comments) before the table body.
            continue
        if line.startswith(_TABLE_END):
            in_table = False
            continue
        if line.startswith("#"):
            continue

        if match := _AVAILABLE_RE.match(line):
            available.add(match.group(1))
        elif match := _CONDITIONAL_RE.match(line):
            conditional.add(match.group(1))
        elif match := _UNAVAILABLE_RE.match(line):
            unavailable.add(match.group(1))
        else:
            raise ValueError(
                f"{table}:{lineno}: unrecognised operator table line: {raw_line.strip()!r}"
            )

    if not seen_table:
        raise ValueError(f"{table}: no {_TABLE_START} block found")
    return OpTable(
        available=frozenset(available),
        conditional=frozenset(conditional),
        unavailable=frozenset(unavailable),
    )


def extract_model_ops(model_path: str | Path) -> set[str] | None:
    """Extract the operators required by a model from its companion .txt report.

    The ``.txt`` metadata report (same basename, ``.txt`` extension) lists the
    unique operator types used by the model, e.g.::

        Unique operator types: 18
          ADD: 3
          REDUCE_MIN: 1

    Args:
        model_path: path to the ``.tflite`` model file.

    Returns:
        A set of operator names, or ``None`` when no companion ``.txt`` file
        exists (or it contains no operator list). The device-side runtime check
        remains the final safety net in that case.
    """
    txt_path = Path(model_path).with_suffix(".txt")
    if not txt_path.exists():
        return None
    content = txt_path.read_text(encoding="utf-8").replace("\r\n", "\n")
    section = re.search(r"Unique operator types:.*?(?=\n\s*\n|\Z)", content, re.DOTALL)
    if not section:
        return None
    ops: set[str] = set()
    for line in section.group(0).splitlines():
        match = _OP_COUNT_RE.match(line)
        if match:
            ops.add(match.group(1))
    return ops or None
