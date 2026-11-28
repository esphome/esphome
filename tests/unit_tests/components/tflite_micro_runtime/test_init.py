"""Tests for the tflite_micro_runtime core configuration and build defines."""

import asyncio
from contextlib import ExitStack
from pathlib import Path
from unittest.mock import AsyncMock, MagicMock, patch
import zlib

import pytest

from esphome import cpp_types as ct
from esphome.components import tflite_micro_runtime as tflm
from esphome.components.tflite_model_io.model_reader import ModelReadError
import esphome.config_validation as cv
from esphome.const import CONF_RAW_DATA_ID
from esphome.core import CORE, ID, EsphomeError
from esphome.types import ConfigType

MODEL_PATH = Path("/models/digit.tflite")
MODEL_BYTES = b"\x1c\x00\x00\x00"


def _model_id() -> ID:
    return ID("tflite_model", is_declaration=True, type=tflm.TFLiteMicroHelper)


def _model_entry() -> ConfigType:
    """A validated model entry, as any component built on the runtime stores it."""
    return {
        tflm.CONF_ID: _model_id(),
        "model": "model.tflite",
        CONF_RAW_DATA_ID: ID("model_data", is_declaration=True, type=ct.uint8),
    }


def _run_to_code(
    config: ConfigType,
    *,
    model_bytes: bytes = MODEL_BYTES,
    estimate: object | None = None,
    estimate_error: Exception | None = None,
) -> tuple[MagicMock, MagicMock, MagicMock]:
    """Run to_code() with the model and every emitter patched.

    Returns the mocked codegen module, the created variable and the estimate mock.
    """
    var = MagicMock()
    with ExitStack() as stack:
        stack.enter_context(
            patch.object(
                tflm, "resolve_model_source", return_value=(MODEL_PATH, model_bytes)
            )
        )
        stack.enter_context(
            patch.object(tflm, "emit_model_buffer", return_value=MagicMock())
        )
        stack.enter_context(patch.object(tflm, "_add_build_defines"))
        stack.enter_context(patch.object(tflm, "add_model_operator_defines"))
        stack.enter_context(patch.object(tflm, "esp32"))
        codegen = stack.enter_context(patch.object(tflm, "cg"))
        if estimate_error is not None:
            estimate_mock = stack.enter_context(
                patch.object(tflm, "estimate_tensor_arena", side_effect=estimate_error)
            )
        else:
            estimation = (
                estimate
                if estimate is not None
                else MagicMock(bytes=1234, peak_activation_bytes=1024, tensor_count=3)
            )
            estimate_mock = stack.enter_context(
                patch.object(tflm, "estimate_tensor_arena", return_value=estimation)
            )
        codegen.new_Pvariable.return_value = var
        codegen.register_component = AsyncMock()
        asyncio.run(tflm.to_code(config))
    return codegen, var, estimate_mock


def test_build_defines_emit_the_helper_flag() -> None:
    with patch("esphome.codegen.add_define") as add_define:
        tflm._add_build_defines()

    assert [call.args[0] for call in add_define.call_args_list] == [
        "USE_TFLITE_MICRO_HELPER"
    ]


def test_only_entries_with_a_runtime_id_are_models(setup_core: Path) -> None:
    """A display or gas sensor also has a `model` key, but is not one of ours."""
    CORE.config = {
        "tflite_image": [_model_entry()],
        "ili9xxx": [
            {
                tflm.CONF_ID: ID("display", is_declaration=True, type=ct.uint8),
                "model": "ILI9341",
            }
        ],
        "esphome": {"name": "test"},
    }
    with patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")):
        paths = tflm._configured_model_paths()

    assert paths == [MODEL_PATH]


def test_final_validate_checks_the_model_operators(setup_core: Path) -> None:
    model_io = MagicMock(inputs=[object()], outputs=[object()])
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "validate_model_operators") as check,
        patch.object(tflm, "read_model_io", return_value=model_io),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({})

    check.assert_called_once_with(MODEL_PATH)


def test_a_model_with_too_many_tensors_is_rejected(setup_core: Path) -> None:
    """A pipeline or decoder holds the tensors in fixed arrays."""
    model_io = MagicMock(inputs=[object()] * 9, outputs=[object()])
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "validate_model_operators"),
        patch.object(tflm, "read_model_io", return_value=model_io),
        pytest.raises(cv.Invalid, match="at most 8"),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({})


def test_to_code_gives_each_model_its_own_checksum(setup_core: Path) -> None:
    """MULTI_CONF: the checksum is per instance, not build-wide."""
    _, first, _ = _run_to_code({tflm.CONF_ID: _model_id()}, model_bytes=b"\x01")
    _, second, _ = _run_to_code({tflm.CONF_ID: _model_id()}, model_bytes=b"\x02\x03")

    assert first.set_model.call_args.args[1:] == (1, zlib.crc32(b"\x01") & 0xFFFFFFFF)
    assert second.set_model.call_args.args[1:] == (
        2,
        zlib.crc32(b"\x02\x03") & 0xFFFFFFFF,
    )


def test_to_code_passes_an_explicit_arena_size_through(setup_core: Path) -> None:
    """An explicit size is used as given; the model is not measured at all."""
    _, var, estimate = _run_to_code(
        {tflm.CONF_ID: _model_id(), tflm.CONF_TENSOR_ARENA_SIZE: 65536}
    )

    var.set_tensor_arena_size.assert_called_once_with(65536)
    estimate.assert_not_called()


def test_to_code_seeds_the_arena_from_the_estimate(setup_core: Path) -> None:
    """Without an explicit size the emitted value is the build-time estimate."""
    estimation = MagicMock(bytes=9876, peak_activation_bytes=4321, tensor_count=7)
    _, var, estimate = _run_to_code({tflm.CONF_ID: _model_id()}, estimate=estimation)

    estimate.assert_called_once_with(MODEL_PATH)
    var.set_tensor_arena_size.assert_called_once_with(9876)


def test_to_code_reports_an_unreadable_model_with_a_hint(setup_core: Path) -> None:
    """A model the estimate cannot size is a config error naming the way around it."""
    with pytest.raises(EsphomeError, match="tensor_arena_size"):
        _run_to_code(
            {tflm.CONF_ID: _model_id()},
            estimate_error=ModelReadError("tensor data has no size"),
        )


def test_to_code_only_enables_debug_when_asked(setup_core: Path) -> None:
    _, var, _ = _run_to_code({tflm.CONF_ID: _model_id(), tflm.CONF_DEBUG: True})
    var.set_debug.assert_called_once_with(True)

    _, plain, _ = _run_to_code({tflm.CONF_ID: _model_id()})
    plain.set_debug.assert_not_called()
