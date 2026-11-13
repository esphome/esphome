"""Component to manage TFLite Micro models and inference runtime.

Image (single-shot) and audio (streaming) models, on top of esp-tflite-micro.
"""

from dataclasses import dataclass
import logging
from pathlib import Path
import zlib

import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.tflite_model_source import (
    infer_model_config_from_filename,
    parse_audio_txt_file,
    parse_model_txt_file,
    resolve_model_source,
)
from esphome.components.tflite_op_processor import (
    emit_all_ops,
    emit_selective_op_defines,
)
from esphome.components.tflite_op_processor.model_reader import (
    ModelInput,
    estimate_tensor_arena,
    read_model_input,
)
from esphome.components.tflite_op_processor.op_manifest import (
    extract_model_ops,
    parse_op_table,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_ID, CONF_RAW_DATA_ID
from esphome.core import CORE, EsphomeError, HexInt
from esphome.types import ConfigType

from . import schema as _schema
from .schema import (
    CONF_FEATURE_COUNT,
    CONF_FEATURES_STEP_SIZE,
    CONF_INPUT_CHANNELS,
    CONF_INPUT_HEIGHT,
    CONF_INPUT_ORDER,
    CONF_INPUT_TYPE,
    CONF_INPUT_WIDTH,
    CONF_INVERT,
    CONF_MODEL_TYPE,
    CONF_NORMALIZE,
    CONF_OUTPUT_PROCESSING,
    CONF_PROBABILITY_CUTOFF,
    CONF_SCALE_FACTOR,
    CONF_SLIDING_WINDOW_SIZE,
    CONF_TENSOR_ARENA_SIZE,
    INPUT_ORDERS,
    INPUT_TYPES,
    MODEL_TYPES,
    OUTPUT_PROCESSING_MODES,
    REPORT_VALUES_SCHEMAS,
    TFLiteMicroHelper,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@nliaudat"]

DOMAIN = "tflite_micro_runtime"

# The build-wide resolver size, in operator slots.
_MAX_OPERATORS_DEFINE = "TFLITE_MICRO_MAX_OPERATORS"


@dataclass
class _RuntimeData:
    """Build-wide state of this configuration run."""

    operator_defines_emitted: bool = False


def _runtime_data() -> _RuntimeData:
    if (data := CORE.data.get(DOMAIN)) is None:
        data = CORE.data[DOMAIN] = _RuntimeData()
    return data


# Library-only sub-components, auto-loaded so users only configure this component.
AUTO_LOAD = [
    "tflite_memory_handler",
    "tflite_model_debug",
    "tflite_model_loader",
    "tflite_model_source",
    "tflite_op_processor",
    "tflite_output_processor",
]

# Attributes consumed by the ESPHome config loader.
CONFIG_SCHEMA = _schema.CONFIG_SCHEMA
MULTI_CONF = _schema.MULTI_CONF
DEPENDENCIES = _schema.DEPENDENCIES + ["tflite_model_source"]

# Re-exported for consumers which reference the runtime class through this component.
__all__ = [
    "TFLiteMicroHelper",
    "CONFIG_SCHEMA",
    "FINAL_VALIDATE_SCHEMA",
    "MULTI_CONF",
    "to_code",
]


async def to_code(config: ConfigType) -> None:
    _add_build_defines()
    model_type = config[CONF_MODEL_TYPE]
    model_path, model_data = resolve_model_source(config)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    prog_arr = _emit_model_buffer(config, model_data)
    cg.add(var.set_model(prog_arr, len(model_data)))
    # Per-instance CRC32 - MULTI_CONF safe, each helper carries its own checksum.
    cg.add(var.set_expected_crc32(zlib.crc32(model_data) & 0xFFFFFFFF))
    cg.add(var.set_model_type(MODEL_TYPES[model_type]))
    cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
    cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
    cg.add_build_flag("-DESP_NN")
    cg.add_build_flag("-DOPTIMIZED_KERNEL=esp_nn")
    # Operator defines are process-global, so they cover ALL configured models.
    _add_operator_defines()
    if config.get(CONF_DEBUG, False):
        cg.add(var.set_debug(True))
    if model_type == "image":
        await _configure_image_model(config, var, model_path)
    elif model_type == "audio":
        await _configure_audio_model(config, var, model_path)
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.4.1")
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.5.0")
    if model_type == "audio":
        esp32.add_idf_component(name="esphome/esp-micro-speech-features", ref="1.2.3")


def _model_filename(model_path: Path) -> str:
    """File name of the resolved model, with Windows separators normalised."""
    return Path(str(model_path).replace("\\", "/")).name


def _emit_model_buffer(config: ConfigType, model_data: bytes) -> cg.MockObj:
    """Emit the model as a PROGMEM array and return a reference to it."""
    # TFLM casts the buffer to a flatbuffer, so it cannot be byte-aligned.
    return cg.progmem_array(
        config[CONF_RAW_DATA_ID], [HexInt(x) for x in model_data], align=16
    )


def _validate_model_operators(model_path: Path) -> None:
    """Reject a model whose operators the pinned TFLM cannot register.

    The model file always lists its operator codes, so an unreadable one is a
    configuration error rather than a load failure on the device.
    """
    try:
        model_ops = extract_model_ops(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    supported_ops = parse_op_table().supported
    if unsupported_ops := sorted(model_ops - supported_ops):
        raise cv.Invalid(
            f"Model '{_model_filename(model_path)}' requires operator(s) "
            f"{', '.join(unsupported_ops)} which esp-tflite-micro 1.4.1 "
            f"does not support.\n"
            f"  Supported operators: {', '.join(sorted(supported_ops))}\n"
            f"  Re-export the model without the unsupported operator(s) "
            f"or choose a compatible model."
        )


def _parse_report(model_path: Path, model_type: str) -> ConfigType | None:
    """Parse the model's optional .txt report, or None when it has none."""
    if model_type == "image":
        return parse_model_txt_file(model_path)
    return parse_audio_txt_file(model_path)


def _validate_report_values(
    values: ConfigType,
    model_type: str,
    model_path: Path,
    source: str = "the .txt report",
) -> ConfigType:
    """Validate the values a model's .txt report supplies."""
    try:
        return REPORT_VALUES_SCHEMAS[model_type](values)
    except cv.Invalid as err:
        # Re-raising as text loses the key path, so repeat it here.
        where = "".join(f"[{key}]" for key in err.path)
        raise cv.Invalid(
            f"Invalid value in {source} of model "
            f"'{_model_filename(model_path)}'{f' {where}' if where else ''}: {err}"
        ) from err


def _report_values(model_path: Path, model_type: str) -> ConfigType | None:
    """Validated .txt report values, or None without a report."""
    if (report := _parse_report(model_path, model_type)) is None:
        return None
    return _validate_report_values(report, model_type, model_path)


def _final_validate(config: ConfigType) -> None:
    """Check the model itself once every component has validated."""
    model_type = config[CONF_MODEL_TYPE]
    model_path, _ = resolve_model_source(config)
    _validate_model_operators(model_path)
    if model_type == "image":
        # Also rejects a model whose input is not an image tensor.
        _resolve_image_config(config, model_path)
        return
    # Also rejects a model whose input is not a streaming one.
    _audio_feature_count(model_path)
    _report_values(model_path, model_type)


FINAL_VALIDATE_SCHEMA = _final_validate


def _add_build_defines() -> None:
    """Emit the defines that describe the build rather than one model."""
    cg.add_define("USE_TFLITE_MICRO_HELPER")


def _emit_max_operators(value: int) -> None:
    """Queue the resolver size, widening a value another component already set."""
    for define in list(CORE.defines):
        if define.name != _MAX_OPERATORS_DEFINE:
            continue
        try:
            existing = int(str(define.value))
        except (TypeError, ValueError) as err:
            raise EsphomeError(
                f"{_MAX_OPERATORS_DEFINE} was already set to {define.value!r}, "
                f"which is not a number; this build needs {value}"
            ) from err
        if existing >= value:
            return
        CORE.defines.discard(define)
        _LOGGER.info(
            "Raising %s from %d to %d for the configured models",
            _MAX_OPERATORS_DEFINE,
            existing,
            value,
        )
    cg.add_define(_MAX_OPERATORS_DEFINE, value)


def _configured_model_ops() -> list[set[str]]:
    """The operators each configured model needs, empty outside a config run."""
    if CORE.config is None:
        return []
    return [
        extract_model_ops(resolve_model_source(entry)[0])
        for entry in CORE.config.get(DOMAIN, [])
    ]


def _add_operator_defines() -> None:
    """Emit the operator defines once for the whole build."""
    data = _runtime_data()
    if data.operator_defines_emitted:
        return
    data.operator_defines_emitted = True
    model_ops = _configured_model_ops()
    if not model_ops:
        # Without a configured model neither the size nor the kernels can be narrowed.
        emit_all_ops()
        return
    # One slot per distinct operator code, so the widest model sizes the resolver.
    _emit_max_operators(max(len(ops) for ops in model_ops))
    emit_selective_op_defines(sorted(set().union(*model_ops)))


# Keys a .txt report supplies under the same name as the YAML option.
_IMAGE_REPORT_KEYS = (
    CONF_INPUT_TYPE,
    CONF_INPUT_CHANNELS,
    CONF_INPUT_WIDTH,
    CONF_INPUT_HEIGHT,
    CONF_OUTPUT_PROCESSING,
    CONF_SCALE_FACTOR,
    CONF_INPUT_ORDER,
    CONF_NORMALIZE,
    CONF_INVERT,
    CONF_TENSOR_ARENA_SIZE,
)

_AUDIO_REPORT_KEYS = (
    CONF_TENSOR_ARENA_SIZE,
    CONF_PROBABILITY_CUTOFF,
    CONF_SLIDING_WINDOW_SIZE,
    CONF_FEATURES_STEP_SIZE,
    CONF_FEATURE_COUNT,
)


def _merge_report_values(
    values: ConfigType, entry: ConfigType, keys: tuple[str, ...]
) -> None:
    """Overwrite report values with the YAML ones for every key both may set."""
    for key in keys:
        if (value := entry.get(key)) is not None:
            values[key] = value


def _model_input(model_path: Path) -> ModelInput:
    """The input tensor a model declares, or a configuration error saying why not."""
    try:
        return read_model_input(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err


def _image_input(model_path: Path) -> ConfigType:
    """The image values a model's input tensor supplies."""
    model_input = _model_input(model_path)
    shape = model_input.shape
    if len(shape) != 4 or shape[0] != 1:
        raise cv.Invalid(
            f"Model '{_model_filename(model_path)}' declares the input shape "
            f"{list(shape)}, not the [1, height, width, channels] layout an image "
            f"model needs"
        )
    return {
        CONF_INPUT_TYPE: model_input.type,
        CONF_INPUT_CHANNELS: shape[3],
        CONF_INPUT_WIDTH: shape[2],
        CONF_INPUT_HEIGHT: shape[1],
    }


def _audio_feature_count(model_path: Path) -> int:
    """The features per frame a streaming model's input tensor declares."""
    shape = _model_input(model_path).shape
    if len(shape) != 3 or shape[0] != 1:
        raise cv.Invalid(
            f"Model '{_model_filename(model_path)}' declares the input shape "
            f"{list(shape)}, not the [1, frames, features] layout a streaming model "
            f"needs"
        )
    return shape[2]


def _resolve_image_config(entry: ConfigType, model_path: Path) -> ConfigType:
    """The image configuration: the model, then its report, then the YAML values."""
    if (values := _report_values(model_path, "image")) is not None:
        auto_config = values
    else:
        auto_config = _validate_report_values(
            infer_model_config_from_filename(_model_filename(model_path)),
            "image",
            model_path,
            source="the file name conventions",
        )
    # The model is authoritative for its own input tensor, so its shape wins over a
    # report that may be stale and supplies what no report or file name can. It is
    # validated like a report value, so a model the image pipeline cannot fill is
    # reported instead of being configured.
    auto_config.update(
        _validate_report_values(
            _image_input(model_path), "image", model_path, source="the input tensor"
        )
    )
    if auto_config[CONF_INPUT_CHANNELS] == 1:
        auto_config[CONF_INPUT_ORDER] = "GRAY"  # one channel has no colour order
    _merge_report_values(auto_config, entry, _IMAGE_REPORT_KEYS)
    return auto_config


async def _configure_image_model(
    entry: ConfigType, var: cg.MockObj, model_path: Path
) -> None:
    auto_config = _resolve_image_config(entry, model_path)
    _LOGGER.info(
        "Image configuration for '%s': %s", _model_filename(model_path), auto_config
    )
    cg.add(var.set_input_type(INPUT_TYPES[auto_config[CONF_INPUT_TYPE]]))
    cg.add(var.set_input_channels(auto_config[CONF_INPUT_CHANNELS]))
    cg.add(var.set_input_width(auto_config[CONF_INPUT_WIDTH]))
    cg.add(var.set_input_height(auto_config[CONF_INPUT_HEIGHT]))
    # Through the table, so C++ only receives a mode tflite_output_processor implements.
    cg.add(
        var.set_output_processing(
            OUTPUT_PROCESSING_MODES[auto_config.get(CONF_OUTPUT_PROCESSING, "argmax")]
        )
    )
    cg.add(var.set_scale_factor(auto_config.get(CONF_SCALE_FACTOR, 1.0)))
    cg.add(var.set_input_order(INPUT_ORDERS[auto_config.get(CONF_INPUT_ORDER, "RGB")]))
    cg.add(var.set_normalize(auto_config.get(CONF_NORMALIZE, False)))
    cg.add(var.set_invert(auto_config.get(CONF_INVERT, False)))
    if (arena_size := auto_config.get(CONF_TENSOR_ARENA_SIZE)) is not None:
        cg.add(var.set_tensor_arena_size(arena_size))
    else:
        # A seed only: the device measures what the model needs and allocates that.
        estimate = estimate_tensor_arena(model_path)
        _LOGGER.info(
            "Estimated tensor arena for '%s': %d bytes (%d peak activation bytes "
            "over %d tensors)",
            _model_filename(model_path),
            estimate.bytes,
            estimate.peak_activation_bytes,
            estimate.tensor_count,
        )
        cg.add(var.set_tensor_arena_size(estimate.bytes))


async def _configure_audio_model(
    entry: ConfigType, var: cg.MockObj, model_path: Path
) -> None:
    # The model carries the feature count; the rest of the streaming settings are its
    # consumer's, so they come from the report or the YAML.
    model_features = _audio_feature_count(model_path)
    report = _report_values(model_path, "audio")
    if report is None:
        _LOGGER.debug(
            "No audio metadata for '%s'; the audio options come from the YAML "
            "configuration or the C++ defaults",
            _model_filename(model_path),
        )
        report = {}
    _merge_report_values(report, entry, _AUDIO_REPORT_KEYS)
    # No estimate: the consumer sizes a streaming model's arena from its manifest.
    if (arena_size := report.get(CONF_TENSOR_ARENA_SIZE)) is not None:
        cg.add(var.set_tensor_arena_size(arena_size))
    if (cutoff := report.get(CONF_PROBABILITY_CUTOFF)) is not None:
        cg.add(var.set_probability_cutoff(cutoff))
    if (window := report.get(CONF_SLIDING_WINDOW_SIZE)) is not None:
        cg.add(var.set_sliding_window_size(window))
    if (step := report.get(CONF_FEATURES_STEP_SIZE)) is not None:
        cg.add(var.set_features_step_size(step))
    cg.add(var.set_feature_count(report.get(CONF_FEATURE_COUNT) or model_features))
