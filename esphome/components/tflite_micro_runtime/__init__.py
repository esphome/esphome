"""Component to manage TFLite Micro models and inference runtime.

Supports image (single-shot) and audio (streaming) model types,
with local file, github shorthand, and http URL model sources.

C++ runtime for https://github.com/espressif/esp-tflite-micro.
Model source resolution and metadata parsing live in the
tflite_model_source component.
"""

import logging
from pathlib import Path
import re
import zlib

import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.tflite_model_source import (
    infer_model_config_from_filename,
    parse_audio_txt_file,
    parse_model_txt_file,
    resolve_model_source,
)
from esphome.components.tflite_op_processor.op_manifest import (
    build_selective_op_defines,
    extract_model_ops,
    parse_op_table,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_ID, CONF_RAW_DATA_ID
from esphome.core import CORE, HexInt
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
    TFLiteMicroHelper,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@nliaudat"]

DOMAIN = "tflite_micro_runtime"

# Operator slots kept when a model has no .txt report to count them from.
_MAX_OPERATORS_FLOOR = 30

# "Total operations: N" in a model's .txt report.
_TOTAL_OPERATIONS_RE = re.compile(r"Total operations:\s+(\d+)")

# Library-only sub-components carrying the split C++ sources of the former
# monolithic component. Auto-loaded so users only configure tflite_micro_runtime.
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

# Re-exported for consumers which reference the runtime class through this
# component. The class is a Component on its own, so a configuration without such
# a consumer still loads its model in setup() and reports it in dump_config().
__all__ = [
    "TFLiteMicroHelper",
    "CONFIG_SCHEMA",
    "MULTI_CONF",
    "to_code",
]


async def to_code(config: ConfigType) -> None:
    _add_build_defines()
    model_type = config[CONF_MODEL_TYPE]
    model_path, model_data = resolve_model_source(config)
    # Compile-time operator support check: fail config generation (before
    # compiling/flashing) when the model requires an operator the TFLM operator
    # table cannot register. The device-side check remains the final safety net
    # when no .txt report is available.
    _validate_model_operators(model_path)
    var = cg.new_Pvariable(config[CONF_ID])
    # Registers the helper with the App, which is what makes setup() (model load)
    # and dump_config() run.
    await cg.register_component(var, config)
    rhs = [HexInt(x) for x in model_data]
    prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
    cg.add(var.set_model(prog_arr, len(model_data)))
    # Per-instance CRC32 - MULTI_CONF safe, each TFLiteMicroHelper carries its own expected checksum
    crc32_val = zlib.crc32(model_data) & 0xFFFFFFFF
    cg.add(var.set_expected_crc32(crc32_val))
    cg.add(var.set_model_type(MODEL_TYPES[model_type]))
    cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
    cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
    cg.add_build_flag("-DESP_NN")
    cg.add_build_flag("-DOPTIMIZED_KERNEL=esp_nn")
    # The operator defines are process-global compile-time macros, so they are
    # computed across ALL configured models and emitted once for the whole build.
    _add_operator_defines()
    if config.get(CONF_DEBUG, False):
        # Per instance only: set_debug() switches the verbose logging and the
        # extra load diagnostics of this helper. It does not touch the build-wide
        # defines above.
        cg.add(var.set_debug(True))
    if model_type == "image":
        await _configure_image_model(config, var, model_path, model_data)
    elif model_type == "audio":
        await _configure_audio_model(config, var, model_path, model_data)
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.2.3")
    if model_type == "audio":
        esp32.add_idf_component(name="esphome/esp-micro-speech-features", ref="1.2.3")


def _validate_model_operators(model_path: Path) -> None:
    """Reject a model whose operators the pinned TFLM cannot register.

    A model without a .txt report cannot be checked here; the loader's
    device-side check covers it at load time.
    """
    model_ops = extract_model_ops(model_path)
    if model_ops is None:
        _LOGGER.info(
            "No operator report for model '%s'; the operator check runs on the device",
            Path(model_path).name,
        )
        return
    supported_ops = parse_op_table().supported
    if unsupported_ops := sorted(model_ops - supported_ops):
        raise cv.Invalid(
            f"Model '{Path(model_path).name}' requires operator(s) "
            f"{', '.join(unsupported_ops)} which esp-tflite-micro 1.3.7 "
            f"does not support.\n"
            f"  Supported operators: {', '.join(sorted(supported_ops))}\n"
            f"  Re-export the model without the unsupported operator(s) "
            f"or choose a compatible model."
        )


def _add_build_defines() -> None:
    """Emit the defines that describe the build rather than one model.

    ``USE_TFLITE_MICRO_HELPER`` compiles the runtime in and
    ``DEBUG_TFLITE_MICRO_HELPER`` compiles in the optional diagnostics of the
    sibling components. Neither belongs to a model: a model's ``debug:`` only
    calls set_debug(), which switches the verbose logging of that one helper.
    ``cg.add_define()`` compares by identity, so a second MULTI_CONF invocation
    would queue them again; skip them when they are already there.
    """
    for name in ("USE_TFLITE_MICRO_HELPER", "DEBUG_TFLITE_MICRO_HELPER"):
        if not any(define.name == name for define in CORE.defines):
            cg.add_define(name)


def _add_operator_defines() -> None:
    """Emit the operator defines once for the whole build.

    ``cg.add_define()`` wraps the value in an expression object that compares by
    identity, so calling it from every to_code() invocation (MULTI_CONF) would
    write the same ``#define`` several times. Skip the call when the define is
    already queued; every invocation computes the same value anyway.
    """
    if any(define.name == "TFLITE_MICRO_MAX_OPERATORS" for define in CORE.defines):
        return
    cg.add_define("TFLITE_MICRO_MAX_OPERATORS", _compute_max_operators())
    if (selective := _selective_op_defines()) is None:
        return
    # One flag per operator table entry, so tflite_op_processor compiles in only
    # the kernels these models need.
    cg.add_define("USE_TFLITE_MICRO_SELECTIVE_OPS")
    for suffix, enabled in selective.items():
        cg.add_define(f"TFLM_OP_ENABLED_{suffix}", enabled)


def _selective_op_defines() -> dict[str, int] | None:
    """Return the per-operator flags, or None when every operator must stay in.

    The kernels no model uses can be left out only when the operator set of every
    configured model is known at config time. One model without a .txt report
    keeps the whole table: a firmware missing the kernel that model needs would
    fail at registration time on the device instead of at build time.
    """
    if CORE.config is None:
        return None
    ops: set[str] = set()
    for entry in CORE.config.get(DOMAIN, []):
        model_ops = extract_model_ops(resolve_model_source(entry)[0])
        if model_ops is None:
            _LOGGER.info(
                "A configured model has no operator report, so every operator is compiled in"
            )
            return None
        ops |= model_ops
    if not ops:
        return None
    return build_selective_op_defines(ops)


def _compute_max_operators() -> int:
    """Compute the single TFLITE_MICRO_MAX_OPERATORS value for the ENTIRE build.

    Defines are process-global. With MULTI_CONF each to_code() invocation would
    otherwise emit its own value and the last one wins -- silently breaking
    earlier models with a too-small resolver.
    All invocations therefore compute the SAME max across every configured model.

    Returns max(30, ops + 5) across all .txt reports. The floor is not optional: a
    model whose report is missing would otherwise get whatever count the other
    models produced, and its resolver would only fail at registration on the
    device.
    """
    if CORE.config is None:
        return _MAX_OPERATORS_FLOOR
    max_ops = 0
    for entry in CORE.config.get(DOMAIN, []):
        model_path, _ = resolve_model_source(entry)
        txt_path = Path(model_path).with_suffix(".txt")
        if not txt_path.exists():
            _LOGGER.info(
                "No operator report for model '%s'; keeping %d operator slots",
                Path(model_path).name,
                _MAX_OPERATORS_FLOOR,
            )
            continue
        txt_content = txt_path.read_text(encoding="utf-8")
        if ops_match := _TOTAL_OPERATIONS_RE.search(txt_content):
            max_ops = max(max_ops, int(ops_match.group(1)) + 5)
        else:
            _LOGGER.warning(
                "Operator report '%s' has no 'Total operations' line; keeping %d "
                "operator slots",
                txt_path.name,
                _MAX_OPERATORS_FLOOR,
            )
    return max(_MAX_OPERATORS_FLOOR, max_ops)


async def _configure_image_model(entry, var, model_path, model_data):
    model_filename = Path(str(model_path).replace("\\", "/")).name
    auto_config = parse_model_txt_file(model_path)
    if auto_config is not None:
        _LOGGER.info(
            "Image configuration for '%s' from its .txt report: %s",
            model_filename,
            auto_config,
        )
    else:
        auto_config = infer_model_config_from_filename(model_filename)
        _LOGGER.info(
            "No usable .txt report for '%s'; using the filename conventions: %s",
            model_filename,
            auto_config,
        )
    yaml_overrides = {}
    for yaml_key, auto_key in [
        (CONF_INPUT_TYPE, "input_type"),
        (CONF_INPUT_CHANNELS, "input_channels"),
        (CONF_INPUT_WIDTH, "input_width"),
        (CONF_INPUT_HEIGHT, "input_height"),
        (CONF_OUTPUT_PROCESSING, "output_processing"),
        (CONF_SCALE_FACTOR, "scale_factor"),
        (CONF_INPUT_ORDER, "input_order"),
        (CONF_NORMALIZE, "normalize"),
        (CONF_INVERT, "invert"),
    ]:
        if yaml_key in entry:
            yaml_overrides[auto_key] = entry[yaml_key]
    if yaml_overrides:
        auto_config.update(yaml_overrides)
    cg.add(var.set_input_type(INPUT_TYPES[auto_config.get("input_type", "uint8")]))
    cg.add(var.set_input_channels(auto_config.get("input_channels", 3)))
    cg.add(var.set_input_width(auto_config.get("input_width", 32)))
    cg.add(var.set_input_height(auto_config.get("input_height", 20)))
    cg.add(var.set_output_processing(auto_config.get("output_processing", "argmax")))
    cg.add(var.set_scale_factor(auto_config.get("scale_factor", 1.0)))
    cg.add(var.set_input_order(INPUT_ORDERS[auto_config.get("input_order", "RGB")]))
    cg.add(var.set_normalize(auto_config.get("normalize", False)))
    cg.add(var.set_invert(auto_config.get("invert", False)))
    if CONF_TENSOR_ARENA_SIZE in entry:
        cg.add(var.set_tensor_arena_size(entry[CONF_TENSOR_ARENA_SIZE]))
    elif "tensor_arena_size" in auto_config:
        cg.add(var.set_tensor_arena_size(auto_config["tensor_arena_size"]))


async def _configure_audio_model(entry, var, model_path, model_data):
    cg.add_define("USE_TFLITE_STREAMING")
    auto_config = parse_audio_txt_file(model_path)
    if auto_config is None:
        # Every audio parameter then comes from YAML or from the C++ defaults, and
        # a streaming model whose window or feature count is wrong still builds.
        _LOGGER.warning(
            "No audio metadata for '%s'; the audio options come from the YAML "
            "configuration or the C++ defaults",
            Path(str(model_path).replace("\\", "/")).name,
        )
        auto_config = {}
    if CONF_TENSOR_ARENA_SIZE in entry:
        cg.add(var.set_tensor_arena_size(entry[CONF_TENSOR_ARENA_SIZE]))
    elif "tensor_arena_size" in auto_config:
        cg.add(var.set_tensor_arena_size(auto_config["tensor_arena_size"]))
    if CONF_PROBABILITY_CUTOFF in entry:
        cg.add(var.set_probability_cutoff(entry[CONF_PROBABILITY_CUTOFF]))
    elif "probability_cutoff" in auto_config:
        cg.add(var.set_probability_cutoff(auto_config["probability_cutoff"]))
    if CONF_SLIDING_WINDOW_SIZE in entry:
        cg.add(var.set_sliding_window_size(entry[CONF_SLIDING_WINDOW_SIZE]))
    elif "sliding_window_size" in auto_config:
        cg.add(var.set_sliding_window_size(auto_config["sliding_window_size"]))
    if CONF_FEATURES_STEP_SIZE in entry:
        cg.add(var.set_features_step_size(entry[CONF_FEATURES_STEP_SIZE]))
    elif "features_step_size" in auto_config:
        cg.add(var.set_features_step_size(auto_config["features_step_size"]))
    if CONF_FEATURE_COUNT in entry:
        cg.add(var.set_feature_count(entry[CONF_FEATURE_COUNT]))
    elif "feature_count" in auto_config:
        cg.add(var.set_feature_count(auto_config["feature_count"]))
