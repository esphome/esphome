"""Tests for the tflite_op_processor operator manifest helpers."""

import re

import pytest

from esphome.components.tflite_op_processor.op_manifest import (
    OpTable,
    extract_model_ops,
    parse_op_table,
    table_path,
)

MODEL_REPORT = """Model report
=================
Unique operator types: 3
  ADD: 2
  REDUCE_MIN: 1
  SOFTMAX: 1

Recommended tensor_arena_size: 64KB
"""


def test_parse_op_table_returns_all_three_sets():
    table = parse_op_table()

    assert isinstance(table, OpTable)
    assert "SOFTMAX" in table.available
    assert "REDUCE_MIN" in table.conditional
    assert "POW" in table.unavailable


def test_supported_ops_include_conditional_ops():
    table = parse_op_table()

    assert "REDUCE_MIN" in table.supported
    assert "DYNAMIC_UPDATE_SLICE" in table.supported
    assert "POW" not in table.supported


def test_operator_sets_are_disjoint():
    table = parse_op_table()

    assert not table.available & table.conditional
    assert not table.available & table.unavailable
    assert not table.conditional & table.unavailable


def test_conditional_ops_have_a_resolver_trait():
    """Every TFLM_OP_CONDITIONAL entry has a matching has_<method> trait."""

    def code_only(path):
        return "\n".join(
            line.split("//", 1)[0]
            for line in path.read_text(encoding="utf-8").splitlines()
        )

    table = table_path()
    traits = set(
        re.findall(
            r"^TFLITE_MICRO_HELPER_DEFINE_HAS_METHOD\((\w+)\)$",
            code_only(table.parent / "op_resolver.h"),
            re.MULTILINE,
        )
    )
    methods = set(
        re.findall(
            r"^TFLM_OP_CONDITIONAL\(\w+,\s*(\w+)\)$", code_only(table), re.MULTILINE
        )
    )

    assert methods == traits
    assert len(parse_op_table().conditional) == len(methods)


def test_unknown_table_line_is_rejected(tmp_path):
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


def test_inline_comments_are_allowed(tmp_path):
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


def test_table_without_active_block_is_rejected(tmp_path):
    table = tmp_path / "tflm_operators.h"
    table.write_text("// no operator table here\n", encoding="utf-8")

    with pytest.raises(ValueError, match="TFLM_OPERATORS_ACTIVE"):
        parse_op_table(table)


def test_extract_model_ops(tmp_path):
    model = tmp_path / "model.tflite"
    (tmp_path / "model.txt").write_text(MODEL_REPORT, encoding="utf-8")

    assert extract_model_ops(model) == {"ADD", "REDUCE_MIN", "SOFTMAX"}


def test_extract_model_ops_handles_crlf(tmp_path):
    model = tmp_path / "model.tflite"
    (tmp_path / "model.txt").write_bytes(MODEL_REPORT.replace("\n", "\r\n").encode())

    assert extract_model_ops(model) == {"ADD", "REDUCE_MIN", "SOFTMAX"}


def test_extract_model_ops_without_report(tmp_path):
    assert extract_model_ops(tmp_path / "model.tflite") is None


def test_extract_model_ops_without_operator_section(tmp_path):
    model = tmp_path / "model.tflite"
    (tmp_path / "model.txt").write_text(
        "Recommended tensor_arena_size: 64KB\n", encoding="utf-8"
    )

    assert extract_model_ops(model) is None
