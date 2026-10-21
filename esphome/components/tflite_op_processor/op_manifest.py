"""Operator manifest helpers for the tflite_op_processor component.

Parses the X-macro operator table (``tflm_operators.h``) so the set of
supported TFLite Micro operators is available to the ESPHome code generator at
compile (config) time. The C++ table remains the single source of truth; this
module only reads it and never edits it.
"""

from __future__ import annotations

from pathlib import Path
import re

# X-macro entry patterns used in tflm_operators.h
_AVAILABLE_RE = re.compile(
    r"^\s*TFLM_OP_AVAILABLE\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)\s*$"
)
_UNAVAILABLE_RE = re.compile(r"^\s*TFLM_OP_UNAVAILABLE\(([A-Za-z0-9_]+)\)\s*$")
# Registered only when the resolver provides the Add method (optional-add);
# esp-tflite-micro 1.3.7 ships kernels for them, so they count as available.
_CONDITIONAL_RE = re.compile(
    r"^\s*TFLM_OP_CONDITIONAL\(([A-Za-z0-9_]+),\s*([A-Za-z0-9_]+)\)\s*$"
)
# A line in the model .txt report listing one operator type, e.g. "  REDUCE_MIN: 1"
_OP_COUNT_RE = re.compile(r"^\s*([A-Z][A-Z0-9_]*):\s+\d+\s*$")


def table_path() -> Path:
    """Return the absolute path of the operator table shipped with this module."""
    return Path(__file__).resolve().parent / "tflm_operators.h"


def parse_op_table(path: str | Path | None = None) -> tuple[set[str], set[str]]:
    """Parse tflm_operators.h into the sets of supported / unsupported ops.

    Args:
        path: optional explicit path to the table (defaults to the table in the
            same directory as this module).

    Returns:
        ``(available, unavailable)``: two sets of ``BuiltinOperator`` suffixes.
        A model may only use operators present in ``available``. Conditional
        entries (``TFLM_OP_CONDITIONAL``) are included in ``available``: the
        device registers them through the optional-add mechanism in
        ``op_resolver.h`` whenever the library resolver provides the method.
    """
    table = Path(path) if path is not None else table_path()
    available: set[str] = set()
    unavailable: set[str] = set()
    with table.open(encoding="utf-8") as f:
        for line in f:
            if line.lstrip().startswith("//"):
                continue
            match = _AVAILABLE_RE.match(line)
            if match:
                available.add(match.group(1))
                continue
            match = _CONDITIONAL_RE.match(line)
            if match:
                available.add(match.group(1))
                continue
            match = _UNAVAILABLE_RE.match(line)
            if match:
                unavailable.add(match.group(1))
    return available, unavailable


def parse_conditional_ops(path: str | Path | None = None) -> set[str]:
    """Return the set of conditional ops (``TFLM_OP_CONDITIONAL`` entries).

    These operators are registered at runtime only when the TFLM resolver
    provides the matching ``Add*`` method; whether that is the case must be
    checked against the library, not assumed from the table.
    """
    table = Path(path) if path is not None else table_path()
    conditional: set[str] = set()
    with table.open(encoding="utf-8") as f:
        for line in f:
            if line.lstrip().startswith("//"):
                continue
            match = _CONDITIONAL_RE.match(line)
            if match:
                conditional.add(match.group(1))
    return conditional


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
