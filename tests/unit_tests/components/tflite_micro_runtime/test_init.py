"""Tests for the tflite_micro_runtime configuration and .txt report handling."""

import asyncio
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from esphome import cpp_types as ct
from esphome.components import tflite_micro_runtime as tflm
from esphome.components.tflite_micro_runtime import schema as tflm_schema
from esphome.components.tflite_op_processor.model_reader import (
    ArenaEstimate,
    ModelInput,
    ModelReadError,
)
import esphome.config_validation as cv
from esphome.core import CORE, ID, Define, EsphomeError

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


# What estimate_tensor_arena() makes of a test model: the seed the device starts
# from before it measures the size the model really needs.
ARENA_ESTIMATE = ArenaEstimate(
    bytes=28672, peak_activation_bytes=12800, tensor_count=31
)

# What read_model_input() makes of a test model. It differs from _image_report()
# so a test can tell which of the two supplied a value.
MODEL_INPUT = ModelInput(height=32, width=20, channels=3, type="uint8")


def test_report_values_are_validated_and_normalised() -> None:
    report = tflm._validate_report_values(
        _image_report(input_type="UINT8", input_width="28", model_version=2),
        "image",
        MODEL_PATH,
    )
    assert report["input_type"] == "uint8"
    assert report["input_width"] == 28
    # Keys the schema does not know are kept: a newer report may carry more.
    assert report["model_version"] == 2


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
        patch.object(tflm, "extract_model_ops", return_value={"ADD"}),
        patch.object(tflm, "parse_op_table", return_value=MagicMock(supported={"ADD"})),
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
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({tflm.CONF_MODEL_TYPE: "image"})


def test_final_validate_rejects_a_model_without_an_image_input(
    setup_core: Path,
) -> None:
    """A streaming model's input has no width or height to configure."""
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "extract_model_ops", return_value={"ADD"}),
        patch.object(tflm, "parse_op_table", return_value=MagicMock(supported={"ADD"})),
        patch.object(tflm, "parse_model_txt_file", return_value=None),
        patch.object(
            tflm,
            "read_model_input",
            side_effect=ModelReadError("digit.tflite: input tensor shape [1, 49, 40]"),
        ),
        pytest.raises(cv.Invalid, match=r"\[1, 49, 40\]"),
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


def test_final_validate_reports_a_broken_operator_report(setup_core: Path) -> None:
    """A report the parser rejects is a config error, not an unexpected exception."""
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(
            tflm,
            "extract_model_ops",
            side_effect=ValueError(
                "digit.txt: the operator list declares zero operators"
            ),
        ),
        pytest.raises(cv.Invalid, match="zero operators"),
    ):
        tflm.FINAL_VALIDATE_SCHEMA({tflm.CONF_MODEL_TYPE: "image"})


def test_yaml_input_geometry_overrides_the_model() -> None:
    var = MagicMock()
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=_image_report()),
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
        patch.object(tflm, "estimate_tensor_arena", return_value=ARENA_ESTIMATE),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(
            tflm._configure_image_model(
                {tflm_schema.CONF_INPUT_WIDTH: 32}, var, MODEL_PATH
            )
        )

    var.set_input_width.assert_called_once_with(32)
    # Everything the YAML does not set comes from the model's input tensor.
    var.set_input_height.assert_called_once_with(32)
    var.set_input_channels.assert_called_once_with(3)


def test_report_values_reach_the_setters_normalised() -> None:
    """The setters get what the schema produced, not the raw report text."""
    var = MagicMock()
    report = _image_report(
        input_type="UINT8", input_width="28", tensor_arena_size="200KB"
    )
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=report),
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model({}, var, MODEL_PATH))

    var.set_input_type.assert_called_once_with(tflm_schema.INPUT_TYPES["uint8"])
    var.set_tensor_arena_size.assert_called_once_with(200 * 1024)


def test_the_model_shape_wins_over_a_stale_report() -> None:
    """The .txt can be left over from another model; the model itself cannot."""
    var = MagicMock()
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=_image_report()),
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
        patch.object(tflm, "estimate_tensor_arena", return_value=ARENA_ESTIMATE),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model({}, var, MODEL_PATH))

    var.set_input_width.assert_called_once_with(MODEL_INPUT.width)
    var.set_input_height.assert_called_once_with(MODEL_INPUT.height)
    var.set_input_channels.assert_called_once_with(MODEL_INPUT.channels)


def test_a_one_channel_model_is_read_in_gray() -> None:
    """A single channel cannot be in a colour order, whatever the file name says."""
    var = MagicMock()
    gray = ModelInput(height=32, width=20, channels=1, type="uint8")
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=None),
        patch.object(tflm, "read_model_input", return_value=gray),
        patch.object(tflm, "estimate_tensor_arena", return_value=ARENA_ESTIMATE),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model({}, var, MODEL_PATH))

    var.set_input_order.assert_called_once_with(tflm_schema.INPUT_ORDERS["GRAY"])


def test_report_output_processing_alias_reaches_the_setter_as_its_mode() -> None:
    var = MagicMock()
    report = _image_report(output_processing="logits_jomjol")
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=report),
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
        patch.object(tflm, "estimate_tensor_arena", return_value=ARENA_ESTIMATE),
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
    entry = {
        tflm_schema.CONF_INPUT_WIDTH: 28,
        tflm_schema.CONF_INPUT_HEIGHT: 28,
    }
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=None),
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
        patch.object(
            tflm,
            "infer_model_config_from_filename",
            return_value={"input_order": "rgb"},
        ),
        patch.object(tflm, "estimate_tensor_arena", return_value=ARENA_ESTIMATE),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model(entry, var, MODEL_PATH))

    var.set_input_order.assert_called_once_with(tflm_schema.INPUT_ORDERS["RGB"])


def test_the_model_supplies_the_shape_no_report_has() -> None:
    """A model without a .txt report or a usable file name is still configurable."""
    var = MagicMock()
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=None),
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
        patch.object(tflm, "infer_model_config_from_filename", return_value={}),
        patch.object(tflm, "estimate_tensor_arena", return_value=ARENA_ESTIMATE),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model({}, var, MODEL_PATH))

    var.set_input_width.assert_called_once_with(MODEL_INPUT.width)
    var.set_input_height.assert_called_once_with(MODEL_INPUT.height)
    var.set_input_type.assert_called_once_with(tflm_schema.INPUT_TYPES["uint8"])


def test_unusable_filename_conventions_are_a_configuration_error() -> None:
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=None),
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
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


def test_selective_operators_are_emitted_by_the_op_processor(setup_core: Path) -> None:
    CORE.config = {tflm.DOMAIN: [{}]}
    with (
        patch.object(tflm, "resolve_model_source", return_value=(MODEL_PATH, b"")),
        patch.object(tflm, "extract_model_ops", return_value={"SOFTMAX", "ADD"}),
        patch.object(tflm, "emit_selective_op_defines") as emit,
        patch("esphome.codegen.add_define"),
    ):
        tflm._add_operator_defines()

    emit.assert_called_once_with(["ADD", "SOFTMAX"])


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


def test_image_arena_is_seeded_from_the_model(setup_core: Path) -> None:
    """Without a configured size the estimate starts the device's own measurement."""
    var = MagicMock()
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=_image_report()),
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
        patch.object(tflm, "estimate_tensor_arena", return_value=ARENA_ESTIMATE),
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model({}, var, MODEL_PATH))

    var.set_tensor_arena_size.assert_called_once_with(ARENA_ESTIMATE.bytes)


def test_a_configured_arena_size_is_not_estimated(setup_core: Path) -> None:
    """The model estimate never overrides what the user or the report asked for."""
    var = MagicMock()
    report = _image_report(tensor_arena_size="200KB")
    with (
        patch.object(tflm, "parse_model_txt_file", return_value=report),
        patch.object(tflm, "read_model_input", return_value=MODEL_INPUT),
        patch.object(tflm, "estimate_tensor_arena") as estimate,
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_image_model({}, var, MODEL_PATH))

    estimate.assert_not_called()
    var.set_tensor_arena_size.assert_called_once_with(200 * 1024)


def test_audio_arena_is_not_estimated(setup_core: Path) -> None:
    """A streaming consumer sizes its own arena from its manifest, not from a guess."""
    var = MagicMock()
    with (
        patch.object(tflm, "parse_audio_txt_file", return_value=None),
        patch.object(tflm, "estimate_tensor_arena") as estimate,
        patch("esphome.codegen.add"),
    ):
        asyncio.run(tflm._configure_audio_model({}, var, MODEL_PATH))

    estimate.assert_not_called()
    var.set_tensor_arena_size.assert_not_called()
