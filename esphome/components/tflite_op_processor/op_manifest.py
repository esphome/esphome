"""Operator manifest helpers for the tflite_op_processor component.

Parses the X-macro operator table (``tflm_operators.h``) so the set of
supported TFLite Micro operators is available to the ESPHome code generator at
compile (config) time, and reads the operator list of a model from the model
file itself. The C++ table remains the single source of truth; this module only
reads it and never edits it.

:func:`build_selective_op_defines` turns a set of operator names into the
``TFLM_OP_ENABLED_<suffix>`` build defines that limit the firmware to the
kernels the configured models use.
"""

from __future__ import annotations

from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path
import re

from .model_reader import read_model_operators

# X-macro entry patterns used in tflm_operators.h. Inline comments are stripped
# before matching, so a trailing "// note" is allowed.
_AVAILABLE_RE = re.compile(r"^TFLM_OP_AVAILABLE\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)$")
_CONDITIONAL_RE = re.compile(
    r"^TFLM_OP_CONDITIONAL\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)$"
)
_UNAVAILABLE_RE = re.compile(r"^TFLM_OP_UNAVAILABLE\(([A-Za-z0-9_]+)\)$")

_TABLE_GUARD = "TFLM_OPERATORS_ACTIVE"
# The block holds plain entries and nothing else. A conditional inside it would
# let the preprocessor drop an entry this parser still reports as supported, and
# version gating already has TFLM_OP_CONDITIONAL plus `if constexpr`.
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

    Args:
        path: optional explicit path to the table (defaults to the table in the
            same directory as this module).

    Returns:
        An :class:`OpTable` with the unconditional, conditional and unavailable
        operator sets. A model may use ``table.supported``.

    Raises:
        ValueError: if the table has no ``TFLM_OPERATORS_ACTIVE`` block, or a
            line in the block is neither a comment nor one of the three entry
            kinds, so the C++ table and this parser cannot drift apart. A
            preprocessor directive other than the closing ``#endif``, or an
            operator entry outside the block, raises too: the C++ table would
            not expand what this parser read, or the other way round.
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
            # File header (pragma, includes, comments) before the table body.
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

    The ``.tflite`` file lists the operator codes it uses, so the model itself is
    authoritative and a companion ``.txt`` report is not consulted. The names are
    the ``BuiltinOperator`` suffixes that :func:`build_selective_op_defines` and
    the C++ operator table use.

    Args:
        model_path: path to the ``.tflite`` model file.

    Returns:
        The distinct operator names the model declares, which is also the number
        of resolver slots it needs.

    Raises:
        ValueError: if the model cannot be read, or declares no operator codes.
            A partial or empty set would compile out kernels the model needs, and
            the device would only find out when it tries to register them.
    """
    return set(read_model_operators(model_path))


def build_selective_op_defines(
    ops: Iterable[str], path: str | Path | None = None
) -> dict[str, int]:
    """Build the ``TFLM_OP_ENABLED_<suffix>`` defines for a set of operators.

    ``op_resolver.h`` reads one flag per registerable operator table entry when
    ``USE_TFLITE_MICRO_SELECTIVE_OPS`` is set, so every one of them gets a value:
    1 when a configured model needs the kernel, 0 when it is compiled out. An
    operator that appears in no flag would make the ``if constexpr`` invalid
    C++, which is why the whole registerable set is always returned.

    Args:
        ops: operator names as :func:`extract_model_ops` returns them, e.g.
            ``{"CONV_2D", "SOFTMAX"}``.
        path: optional explicit path to the table (defaults to the table in the
            same directory as this module).

    Returns:
        A mapping of operator suffix to 0 or 1, covering every registerable
        operator in the table.

    Raises:
        ValueError: if ``ops`` names an operator that is not in the table, so a
            model the firmware cannot run fails at build time instead of at
            registration time on the device. An operator the table marks
            ``TFLM_OP_UNAVAILABLE`` is rejected for the same reason: the
            resolver cannot register it, and flagging it 0 would only turn the
            failure into a runtime one.
    """
    table = parse_op_table(path)
    requested = frozenset(ops)
    if unknown := sorted(requested - table.suffixes):
        raise ValueError(
            f"unknown TFLite Micro operator(s): {', '.join(unknown)}; "
            f"add them to {table_path() if path is None else path}"
        )
    if unavailable := sorted(requested & table.unavailable):
        raise ValueError(
            f"TFLite Micro operator(s) {', '.join(unavailable)} cannot be "
            f"registered by the pinned esp-tflite-micro, as marked in "
            f"{table_path() if path is None else path}; a model that needs them "
            "cannot run"
        )
    return {suffix: int(suffix in requested) for suffix in sorted(table.supported)}
