"""Tests for the tflite_micro_runtime core configuration and build defines."""

from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome import cpp_types as ct
from esphome.components import tflite_micro_runtime as tflm
import esphome.config_validation as cv
from esphome.core import CORE, ID, Define, EsphomeError

MODEL_PATH = Path("/models/digit.tflite")


def test_build_defines_emit_the_helper_flag() -> None:
    with patch("esphome.codegen.add_define") as add_define:
        tflm._add_build_defines()

    assert [call.args[0] for call in add_define.call_args_list] == [
        "USE_TFLITE_MICRO_HELPER"
    ]


def test_the_model_buffer_is_aligned_for_the_flatbuffer_cast(setup_core: Path) -> None:
    """TFLM casts the buffer, so its address must be aligned like one."""
    CORE.main_statements = []

    tflm._emit_model_buffer(
        {tflm.CONF_RAW_DATA_ID: ID("model_data", is_declaration=True, type=ct.uint8)},
        b"\x01\x02",
    )

    assert (
        str(CORE.main_statements[-1])
        == "alignas(16) static constexpr uint8_t model_data[] PROGMEM = {0x01, 0x02};"
    )


def test_every_configured_model_contributes_its_operators(setup_core: Path) -> None:
    """A selective build must contain a kernel whichever model needs it."""
    CORE.config = {tflm.DOMAIN: [{}, {}]}
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(
            tflm, "extract_model_ops", side_effect=[{"ADD"}, {"SOFTMAX", "ADD"}]
        ),
        patch.object(tflm, "emit_selective_op_defines") as emit,
        patch("esphome.codegen.add_define"),
    ):
        tflm._add_operator_defines()

    emit.assert_called_once_with(["ADD", "SOFTMAX"])


def test_the_resolver_is_sized_by_the_widest_model(setup_core: Path) -> None:
    """One resolver slot per distinct operator code, with no margin to spare."""
    CORE.config = {tflm.DOMAIN: [{}, {}]}
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(
            tflm, "extract_model_ops", side_effect=[{"ADD", "SOFTMAX"}, {"ADD"}]
        ),
        patch.object(tflm, "emit_selective_op_defines"),
        patch("esphome.codegen.add_define") as add_define,
    ):
        tflm._add_operator_defines()

    add_define.assert_called_once_with("TFLITE_MICRO_MAX_OPERATORS", 2)


def test_no_configured_models_keep_the_full_operator_table(setup_core: Path) -> None:
    """Outside a config run there is no model to narrow the kernels to."""
    CORE.config = None
    with (
        patch.object(tflm, "emit_all_ops") as emit_all,
        patch.object(tflm, "emit_selective_op_defines") as emit_selective,
        patch("esphome.codegen.add_define") as add_define,
    ):
        tflm._add_operator_defines()

    emit_all.assert_called_once_with()
    emit_selective.assert_not_called()
    add_define.assert_not_called()


def test_operator_defines_are_emitted_once(setup_core: Path) -> None:
    """MULTI_CONF calls to_code() per model, but the defines are build-wide."""
    CORE.config = {tflm.DOMAIN: [{}, {}]}
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "extract_model_ops", return_value={"ADD"}),
        patch.object(tflm, "emit_selective_op_defines"),
        patch("esphome.codegen.add_define") as add_define,
    ):
        tflm._add_operator_defines()
        tflm._add_operator_defines()

    assert [call.args[0] for call in add_define.call_args_list] == [
        "TFLITE_MICRO_MAX_OPERATORS"
    ]


def test_a_larger_operator_count_widens_an_existing_define(
    setup_core: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Another component may have sized the resolver already; the larger value wins."""
    monkeypatch.setattr(CORE, "defines", {Define("TFLITE_MICRO_MAX_OPERATORS", 12)})

    tflm._emit_max_operators(47)

    values = [
        int(str(define.value))
        for define in CORE.defines
        if define.name == "TFLITE_MICRO_MAX_OPERATORS"
    ]
    assert values == [47]


def test_a_smaller_operator_count_keeps_an_existing_define(
    setup_core: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    monkeypatch.setattr(CORE, "defines", {Define("TFLITE_MICRO_MAX_OPERATORS", 60)})

    with patch("esphome.codegen.add_define") as add_define:
        tflm._emit_max_operators(47)

    add_define.assert_not_called()
    assert [str(define.value) for define in CORE.defines] == ["60"]


def test_a_non_numeric_operator_define_is_an_error(
    setup_core: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Silently keeping a value that cannot be compared would hide the problem."""
    monkeypatch.setattr(CORE, "defines", {Define("TFLITE_MICRO_MAX_OPERATORS", "lots")})

    with pytest.raises(EsphomeError, match="lots"):
        tflm._emit_max_operators(47)


def test_final_validate_accepts_a_supported_model(setup_core: Path) -> None:
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "extract_model_ops", return_value={"ADD"}),
        patch.object(tflm, "parse_op_table", return_value=MagicMock(supported={"ADD"})),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({})


def test_final_validate_rejects_an_unsupported_operator(setup_core: Path) -> None:
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "extract_model_ops", return_value={"WHERE"}),
        patch.object(tflm, "parse_op_table", return_value=MagicMock(supported={"ADD"})),
        pytest.raises(cv.Invalid, match="WHERE"),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({})


def test_final_validate_reports_an_unreadable_model(setup_core: Path) -> None:
    """A model whose operators cannot be read is a config error, not a load failure."""
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "extract_model_ops", side_effect=ValueError("not a model")),
        pytest.raises(cv.Invalid, match="not a model"),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({})
