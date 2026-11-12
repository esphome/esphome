"""Operator manifest helpers for the tflite_op_processor component.

Parses the X-macro operator table (``tflm_operators.h``) at config time and reads a
model's operator list from the model file; the C++ table stays the source of truth.
"""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path
import re

from .model_reader import read_model_operators

# X-macro entry patterns of tflm_operators.h, after inline comments are stripped.
_AVAILABLE_RE = re.compile(r"^TFLM_OP_AVAILABLE\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)$")
_CONDITIONAL_RE = re.compile(
    r"^TFLM_OP_CONDITIONAL\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)$"
)
_UNAVAILABLE_RE = re.compile(r"^TFLM_OP_UNAVAILABLE\(([A-Za-z0-9_]+)\)$")

_TABLE_GUARD = "TFLM_OPERATORS_ACTIVE"
# The block holds plain entries only: a conditional inside it could let the
# preprocessor drop an entry this parser still reports as supported.
_ENDIF = "#endif"


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

    @property
    def suffixes(self) -> frozenset[str]:
        """Every operator in the table, whatever its registration kind."""
        return self.available | self.conditional | self.unavailable


def table_path() -> Path:
    """Return the absolute path of the operator table shipped with this module."""
    return Path(__file__).resolve().parent / "tflm_operators.h"


def parse_op_table(path: str | Path | None = None) -> OpTable:
    """Parse tflm_operators.h into the operator sets used by the resolver.
    Raises ``ValueError`` when the table and this parser could drift apart: no
    ``TFLM_OPERATORS_ACTIVE`` block, an unterminated block, a preprocessor
    directive other than the closing ``#endif``, or an operator entry outside it.
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
        if not in_table:
            if line.startswith("#if") and _TABLE_GUARD in line:
                in_table = True
                seen_table = True
                continue
            # An entry outside the block would be compiled out of the C++ table
            # while this parser silently ignored it.
            if _is_op_entry(line):
                raise ValueError(
                    f"{table}:{lineno}: operator entry outside the "
                    f"{_TABLE_GUARD} block: {raw_line.strip()!r}"
                )
            continue
        if line.startswith("#"):
            if not line.startswith(_ENDIF):
                raise ValueError(
                    f"{table}:{lineno}: unsupported preprocessor directive "
                    f"inside the {_TABLE_GUARD} block: {raw_line.strip()!r}"
                )
            in_table = False
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
        raise ValueError(f"{table}: no {_TABLE_GUARD} block found")
    if in_table:
        raise ValueError(f"{table}: unterminated {_TABLE_GUARD} block")
    return OpTable(
        available=frozenset(available),
        conditional=frozenset(conditional),
        unavailable=frozenset(unavailable),
    )


def _is_op_entry(line: str) -> bool:
    """Whether a stripped line is one of the three X-macro entry kinds."""
    return (
        _AVAILABLE_RE.match(line) is not None
        or _CONDITIONAL_RE.match(line) is not None
        or _UNAVAILABLE_RE.match(line) is not None
    )


def extract_model_ops(model_path: str | Path) -> set[str]:
    """Extract the operators a model requires from the model file.
    The ``.tflite`` file is authoritative; a companion ``.txt`` report is not
    consulted. Raises ``ValueError`` when the model cannot be read or declares no
    operator codes, so a partial set cannot compile out a kernel it needs.
    """
    return set(read_model_operators(model_path))


def build_selective_op_defines(
    ops: Iterable[str], path: str | Path | None = None
) -> dict[str, int]:
    """Build the ``TFLM_OP_ENABLED_<suffix>`` defines for a set of operators.
    Every registerable entry gets a value, 1 when a model needs the kernel and 0
    when it is compiled out, because a missing flag would make the ``if constexpr``
    in ``op_resolver.h`` invalid C++. Raises ``ValueError`` for a name that is not in
    the table, or one it marks ``TFLM_OP_UNAVAILABLE``: the resolver cannot register
    it, so the build should say so rather than the device.
    """
    table = parse_op_table(path)
    requested = frozenset(ops)
    where = table_path() if path is None else path
    if unknown := sorted(requested - table.suffixes):
        raise ValueError(
            f"unknown TFLite Micro operator(s): {', '.join(unknown)}; add them to {where}"
        )
    if unavailable := sorted(requested & table.unavailable):
        raise ValueError(
            f"TFLite Micro operator(s) {', '.join(unavailable)} cannot be "
            f"registered by the pinned esp-tflite-micro, as marked in {where}; "
            "a model that needs them cannot run"
        )
    return {suffix: int(suffix in requested) for suffix in sorted(table.supported)}
