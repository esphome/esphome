"""Tests for the tflite_op_processor operator manifest helpers."""

from pathlib import Path

import pytest

from esphome.components import tflite_op_processor
from esphome.components.tflite_op_processor import CONF_OPERATORS
from esphome.components.tflite_op_processor.op_manifest import (
    OpTable,
    build_selective_op_defines,
    extract_model_ops,
    parse_op_table,
)
import esphome.config_validation as cv
from esphome.core import CORE

MODEL_REPORT = """Model report
=================
Unique operator types: 3
  ADD: 2
  REDUCE_MIN: 1
  SOFTMAX: 1

Recommended tensor_arena_size: 64KB
"""


def test_parse_op_table_returns_all_three_sets() -> None:
    table = parse_op_table()

    assert isinstance(table, OpTable)
    assert "SOFTMAX" in table.available
    assert "REDUCE_MIN" in table.conditional
    assert "POW" in table.unavailable


def test_supported_ops_include_conditional_ops() -> None:
    table = parse_op_table()

    assert "REDUCE_MIN" in table.supported
    assert "DYNAMIC_UPDATE_SLICE" in table.supported
    assert "POW" not in table.supported


def test_operator_sets_are_disjoint() -> None:
    table = parse_op_table()

    assert not table.available & table.conditional
    assert not table.available & table.unavailable
    assert not table.conditional & table.unavailable


def test_suffixes_cover_every_table_entry() -> None:
    table = parse_op_table()

    assert table.suffixes == table.available | table.conditional | table.unavailable
    assert len(table.suffixes) == len(table.available) + len(table.conditional) + len(
        table.unavailable
    )


def test_selective_defines_flag_only_the_requested_ops() -> None:
    defines = build_selective_op_defines({"CONV_2D", "SOFTMAX"})

    # Unavailable operators read no flag, so they get none.
    assert set(defines) == parse_op_table().supported
    assert {name for name, value in defines.items() if value} == {
        "CONV_2D",
        "SOFTMAX",
    }
    assert set(defines.values()) <= {0, 1}


def test_selective_defines_without_ops_compile_every_kernel_out() -> None:
    defines = build_selective_op_defines([])

    assert defines
    assert not any(defines.values())


def test_selective_defines_reject_unknown_ops() -> None:
    with pytest.raises(ValueError, match="FROBNICATE"):
        build_selective_op_defines({"SOFTMAX", "FROBNICATE"})


def test_selective_defines_reject_unavailable_ops() -> None:
    """A kernel the table marks unavailable cannot be flagged into the build."""
    with pytest.raises(ValueError, match="POW"):
        build_selective_op_defines({"SOFTMAX", "POW"})


def test_schema_accepts_an_operator_list() -> None:
    config = tflite_op_processor.CONFIG_SCHEMA({CONF_OPERATORS: ["SOFTMAX"]})

    assert config[CONF_OPERATORS] == ["SOFTMAX"]


def test_schema_rejects_an_empty_operator_list() -> None:
    with pytest.raises(cv.Invalid):
        tflite_op_processor.CONFIG_SCHEMA({CONF_OPERATORS: []})


def test_emit_selective_op_defines_reports_an_unavailable_op(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A manifest error reaches the user as a config error, not a traceback."""
    monkeypatch.setattr(CORE, "defines", set())

    with pytest.raises(cv.Invalid, match="POW"):
        tflite_op_processor.emit_selective_op_defines(["POW"])


def test_emit_selective_op_defines_keeps_the_kernels_of_other_components(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A second operator list widens the flags instead of replacing them."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_selective_op_defines(["SOFTMAX"])

    tflite_op_processor.emit_selective_op_defines(["CONV_2D"])

    flags = {
        define.name: str(define.value)
        for define in CORE.defines
        if define.name.startswith("TFLM_OP_ENABLED_")
    }
    names = [
        define.name
        for define in CORE.defines
        if define.name.startswith("TFLM_OP_ENABLED_")
    ]
    assert len(names) == len(set(names))
    assert flags["TFLM_OP_ENABLED_SOFTMAX"] == "1"
    assert flags["TFLM_OP_ENABLED_CONV_2D"] == "1"
    assert flags["TFLM_OP_ENABLED_ADD"] == "0"


def test_emit_selective_op_defines_validates_after_other_components(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Flags emitted earlier do not skip validation of a later operator list."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_selective_op_defines(["SOFTMAX"])

    with pytest.raises(cv.Invalid, match="FROBNICATE"):
        tflite_op_processor.emit_selective_op_defines(["FROBNICATE"])


def test_emit_all_ops_cancels_selective_flags(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A component that cannot name its operators keeps every kernel."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_selective_op_defines(["SOFTMAX"])

    tflite_op_processor.emit_all_ops()

    names = {define.name for define in CORE.defines}
    assert "USE_TFLITE_MICRO_SELECTIVE_OPS" not in names
    assert not [name for name in names if name.startswith("TFLM_OP_ENABLED_")]


def test_emit_all_ops_wins_over_a_later_selective_request(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The result must not depend on the order components run in."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_all_ops()

    tflite_op_processor.emit_selective_op_defines(["SOFTMAX"])

    names = {define.name for define in CORE.defines}
    assert "USE_TFLITE_MICRO_SELECTIVE_OPS" not in names
    assert not [name for name in names if name.startswith("TFLM_OP_ENABLED_")]


def test_emit_selective_op_defines_still_validates_after_emit_all_ops(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """The full kernel set must not hide an operator the table cannot register."""
    monkeypatch.setattr(CORE, "defines", set())
    tflite_op_processor.emit_all_ops()

    with pytest.raises(cv.Invalid, match="POW"):
        tflite_op_processor.emit_selective_op_defines(["POW"])


def test_unknown_table_line_is_rejected(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        "TFLM_OP_AVAILABLE(SOFTMAX, AddSoftmax)\n"
        "TFLM_OP_MAYBE(FROBNICATE)\n"
        "#endif\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="unrecognised operator table line"):
        parse_op_table(table)


def test_inline_comments_are_allowed(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "// operator table\n"
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        "TFLM_OP_AVAILABLE(SOFTMAX, AddSoftmax)  // classification\n"
        "TFLM_OP_UNAVAILABLE(POW)\n"
        "#endif  // TFLM_OPERATORS_ACTIVE\n",
        encoding="utf-8",
    )

    parsed = parse_op_table(table)

    assert parsed.available == frozenset({"SOFTMAX"})
    assert parsed.conditional == frozenset()
    assert parsed.unavailable == frozenset({"POW"})


def test_table_without_active_block_is_rejected(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text("// no operator table here\n", encoding="utf-8")

    with pytest.raises(ValueError, match="TFLM_OPERATORS_ACTIVE"):
        parse_op_table(table)


def test_nested_conditional_is_rejected(tmp_path: Path) -> None:
    """Entries inside a conditional would be reported even when it is false."""
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        "#if TFLM_VERSION >= 2\n"
        "TFLM_OP_AVAILABLE(SOFTMAX, AddSoftmax)\n"
        "#endif\n"
        "TFLM_OP_AVAILABLE(ADD, AddAdd)\n"
        "#endif\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="unsupported preprocessor directive"):
        parse_op_table(table)


def test_operator_entry_outside_the_block_is_rejected(tmp_path: Path) -> None:
    """The C++ table would not expand it, so ignoring it would hide the drift."""
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "TFLM_OP_AVAILABLE(SOFTMAX, AddSoftmax)\n"
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        "TFLM_OP_AVAILABLE(ADD, AddAdd)\n"
        "#endif\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="operator entry outside"):
        parse_op_table(table)


def test_unsupported_directive_inside_the_block_is_rejected(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "#ifdef TFLM_OPERATORS_ACTIVE\n"
        '#include "other_ops.h"\n'
        "TFLM_OP_AVAILABLE(ADD, AddAdd)\n"
        "#endif\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="unsupported preprocessor directive"):
        parse_op_table(table)


def test_unterminated_block_is_rejected(tmp_path: Path) -> None:
    table = tmp_path / "tflm_operators.h"
    table.write_text(
        "#ifdef TFLM_OPERATORS_ACTIVE\nTFLM_OP_AVAILABLE(ADD, AddAdd)\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="unterminated"):
        parse_op_table(table)


def test_extract_model_ops(tmp_path: Path) -> None:
    model = tmp_path / "model.tflite"
    (tmp_path / "model.txt").write_text(MODEL_REPORT, encoding="utf-8")

    assert extract_model_ops(model) == {"ADD", "REDUCE_MIN", "SOFTMAX"}


def test_extract_model_ops_handles_crlf(tmp_path: Path) -> None:
    model = tmp_path / "model.tflite"
    (tmp_path / "model.txt").write_bytes(MODEL_REPORT.replace("\n", "\r\n").encode())

    assert extract_model_ops(model) == {"ADD", "REDUCE_MIN", "SOFTMAX"}


def test_extract_model_ops_without_report(tmp_path: Path) -> None:
    assert extract_model_ops(tmp_path / "model.tflite") is None


def test_extract_model_ops_without_operator_section(tmp_path: Path) -> None:
    model = tmp_path / "model.tflite"
    (tmp_path / "model.txt").write_text(
        "Recommended tensor_arena_size: 64KB\n", encoding="utf-8"
    )

    assert extract_model_ops(model) is None


def test_extract_model_ops_rejects_an_unreadable_entry(tmp_path: Path) -> None:
    """A line the parser cannot read must not shrink the operator set."""
    model = tmp_path / "model.tflite"
    (tmp_path / "model.txt").write_text(
        "Unique operator types: 2\n  ADD: 1\n  softmax (classifier): 1\n\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="cannot read operator entry"):
        extract_model_ops(model)


def test_extract_model_ops_rejects_a_count_mismatch(tmp_path: Path) -> None:
    """The declared count catches entries that were dropped by a bad regex."""
    model = tmp_path / "model.tflite"
    (tmp_path / "model.txt").write_text(
        "Unique operator types: 3\n  ADD: 1\n  SOFTMAX: 1\n\n",
        encoding="utf-8",
    )

    with pytest.raises(ValueError, match="declares 3"):
        extract_model_ops(model)
