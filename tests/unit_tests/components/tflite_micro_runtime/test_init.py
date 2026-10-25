"""Tests for the tflite_micro_runtime configuration and .txt report handling."""

import asyncio
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome.components import tflite_micro_runtime as tflm
from esphome.components.tflite_micro_runtime import schema as tflm_schema
import esphome.config_validation as cv

MODEL_PATH = Path("/models/digit.tflite")


def _image_report(**values: object) -> dict:
    """A .txt report as parse_model_txt_file() returns it."""
    return {
        "input_height": 28,
        "input_width": 28,
        "input_channels": 1,
        "input_type": "uint8",
        **values,
    }


def test_report_values_are_validated_and_normalised() -> None:
    report = tflm._validate_report_values(
        _image_report(input_type="UINT8", input_width="28", max_operators=47),
        "image",
        MODEL_PATH,
    )
    assert report["input_type"] == "uint8"
    assert report["input_width"] == 28
    # Keys the schema does not know are kept: the rest of the build uses them.
    assert report["max_operators"] == 47


@pytest.mark.parametrize(
    ("model_type", "report"),
    [
        ("image", {"input_type": "int8"}),
        ("image", {"output_processing": "warp"}),
        ("image", {"tensor_arena_size": 1024}),
        ("audio", {"probability_cutoff": 128}),
        ("audio", {"feature_count": 5}),
    ],
)
def test_report_value_outside_the_supported_set_names_the_model(
    model_type: str, report: dict
) -> None:
    with pytest.raises(cv.Invalid, match="digit.tflite"):
        tflm._validate_report_values(report, model_type, MODEL_PATH)


def test_final_validate_rejects_a_bad_report(setup_core: Path) -> None:
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "extract_model_ops", return_value=None),
        patch.object(tflm, "parse_model_txt_file", return_value={"input_type": "int8"}),
        pytest.raises(cv.Invalid, match="digit.tflite"),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({tflm.CONF_MODEL_TYPE: "image"})


def test_final_validate_accepts_a_supported_model(setup_core: Path) -> None:
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "extract_model_ops", return_value={"ADD"}),
        patch.object(tflm, "parse_op_table", return_value=MagicMock(supported={"ADD"})),
        patch.object(tflm, "parse_model_txt_file", return_value=_image_report()),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({tflm.CONF_MODEL_TYPE: "image"})


def test_final_validate_rejects_an_unsupported_operator(setup_core: Path) -> None:
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "extract_model_ops", return_value={"ADD", "CUSTOM"}),
        patch.object(tflm, "parse_op_table", return_value=MagicMock(supported={"ADD"})),
        pytest.raises(cv.Invalid, match="CUSTOM"),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({tflm.CONF_MODEL_TYPE: "image"})


def test_yaml_input_geometry_overrides_the_report() -> None:
    var = MagicMock()
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=_image_report()),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(
            tflm._configure_image_model(
                {tflm_schema.CONF_INPUT_WIDTH: 32}, var, MODEL_PATH
            )
        )

    var.set_input_width.assert_called_once_with(32)
    # Everything the YAML does not set still comes from the report.
    var.set_input_height.assert_called_once_with(28)
    var.set_input_channels.assert_called_once_with(1)


def test_report_values_reach_the_setters_normalised() -> None:
    """The setters get what the schema produced, not the raw report text."""
    var = MagicMock()
    report = _image_report(
        input_type="UINT8", input_width="28", tensor_arena_size="200KB"
    )
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=report),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model({}, var, MODEL_PATH))

    var.set_input_type.assert_called_once_with(tflm_schema.INPUT_TYPES["uint8"])
    var.set_input_width.assert_called_once_with(28)
    var.set_tensor_arena_size.assert_called_once_with(200 * 1024)


def test_report_output_processing_alias_reaches_the_setter_as_its_mode() -> None:
    var = MagicMock()
    report = _image_report(output_processing="logits_jomjol")
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=report),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model({}, var, MODEL_PATH))

    var.set_output_processing.assert_called_once_with("softmax")


def test_report_accepts_input_order_and_preprocessing() -> None:
    report = tflm._validate_report_values(
        _image_report(input_order="rgb", normalize="false", invert=True),
        "image",
        MODEL_PATH,
    )
    assert report["input_order"] == "RGB"
    assert report["normalize"] is False
    assert report["invert"] is True


def test_filename_conventions_are_validated() -> None:
    var = MagicMock()
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=None),
        patch.object(
            tflm,
            "infer_model_config_from_filename",
            return_value={"input_order": "rgb"},
        ),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model({}, var, MODEL_PATH))

    var.set_input_order.assert_called_once_with(tflm_schema.INPUT_ORDERS["RGB"])


def test_unusable_filename_conventions_are_a_configuration_error() -> None:
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=None),
        patch.object(
            tflm,
            "infer_model_config_from_filename",
            return_value={"input_type": "int8"},
        ),
        patch("esphome.codegen.add"),
        pytest.raises(cv.Invalid, match="file name conventions"),
    ):
        asyncio.run(tflm._configure_image_model({}, MagicMock(), MODEL_PATH))


def test_audio_configuration_sets_no_build_define() -> None:
    var = MagicMock()
    with (
        patch.object(tflm, "parse_audio_txt_file", return_value=None),
        patch("esphome.codegen.add"),
        patch("esphome.codegen.add_define") as add_define,
    ):
        asyncio.run(
            tflm._configure_audio_model(
                {tflm_schema.CONF_PROBABILITY_CUTOFF: 0.5}, var, MODEL_PATH
            )
        )

    add_define.assert_not_called()
    var.set_probability_cutoff.assert_called_once_with(0.5)


def test_build_defines_emit_the_helper_flags() -> None:
    with patch("esphome.codegen.add_define") as add_define:
        tflm._add_build_defines()

    assert [call.args[0] for call in add_define.call_args_list] == [
        "USE_TFLITE_MICRO_HELPER",
        "DEBUG_TFLITE_MICRO_HELPER",
    ]


def test_yaml_input_type_rejects_int8(setup_core: Path, tmp_path: Path) -> None:
    model = tmp_path / "digit.tflite"
    model.write_bytes(b"")
    config = {
        tflm_schema.CONF_MODEL_TYPE: "image",
        tflm_schema.CONF_MODEL: {"type": "local", "path": str(model)},
        tflm_schema.CONF_INPUT_TYPE: "int8",
    }

    with pytest.raises(cv.Invalid):
        tflm_schema.PER_MODEL_SCHEMA(config)
