"""Parse ``tflm_operators.h`` and read a model's operator list; the C++ table stays the source of truth."""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path
import re

from esphome.components.tflite_model_io.model_reader import read_model_operators

# X-macro entry patterns of tflm_operators.h, after inline comments are stripped.
_AVAILABLE_RE = re.compile(r"^TFLM_OP_AVAILABLE\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)$")
_CONDITIONAL_RE = re.compile(
    r"^TFLM_OP_CONDITIONAL\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)$"
)
_UNAVAILABLE_RE = re.compile(r"^TFLM_OP_UNAVAILABLE\(([A-Za-z0-9_]+)\)$")

_TABLE_GUARD = "TFLM_OPERATORS_ACTIVE"
# The block holds plain entries only: a conditional inside it could let the preprocessor drop an entry this parser
# still reports as supported.
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
    return Path(__file__).resolve().parent / "tflm_operators.h"


def parse_op_table(path: str | Path | None = None) -> OpTable:
    """Parse the table into the operator sets the resolver uses.
    Raises ``ValueError`` on a missing or unterminated block, a stray directive or an entry outside it.
    """
    table = Path(path) if path is not None else table_path()
    available: set[str] = set()
    conditional: set[str] = set()
    unavailable: set[str] = set()
    in_table = False
    seen_table = False
    continued = False
    try:
        lines = table.read_text(encoding="utf-8").splitlines()
    except OSError as err:
        raise ValueError(f"{table}: cannot read the operator table: {err}") from err
    for lineno, raw_line in enumerate(lines, start=1):
        line = raw_line.split("//", 1)[0].strip()
        if continued:
            # The #if guard may wrap onto the next line with a trailing backslash.
            continued = raw_line.rstrip().endswith("\\")
            continue
        if not line:
            continue
        if not in_table:
            if line.startswith("#if") and _TABLE_GUARD in line:
                in_table = True
                seen_table = True
                continued = raw_line.rstrip().endswith("\\")
                continue
            # An entry outside the block would be compiled out of the C++ table while this parser ignored it.
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
    return (
        _AVAILABLE_RE.match(line) is not None
        or _CONDITIONAL_RE.match(line) is not None
        or _UNAVAILABLE_RE.match(line) is not None
    )


def extract_model_ops(model_path: str | Path) -> set[str]:
    """Extract the operators a model requires; the ``.tflite`` file is authoritative, not a report.
    A model that cannot be read or declares no codes raises ``ValueError``.
    """
    return set(read_model_operators(model_path))


def build_selective_op_defines(
    ops: Iterable[str], path: str | Path | None = None
) -> dict[str, int]:
    """Build the ``TFLM_OP_ENABLED_<suffix>`` defines for a set of operators.
    Every registerable entry gets 1 or 0, because a missing flag would make the ``if constexpr`` in
    ``op_resolver.h`` invalid C++. A name not in the table, or one it marks unavailable, raises ``ValueError``.
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
