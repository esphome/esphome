"""Component to manage TFLite Micro models and inference runtime.

Supports image (single-shot) and audio (streaming) model types,
with local file, github shorthand, and http URL model sources.

C++ runtime for https://github.com/espressif/esp-tflite-micro.
Model source resolution and metadata parsing live in the
tflite_model_source component.
"""

from pathlib import Path
import re
import zlib

import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.tflite_model_source import (
    _parse_audio_config,
    infer_model_config_from_filename,
    parse_model_txt_file,
    resolve_model_source,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_ID, CONF_RAW_DATA_ID
from esphome.core import CORE, HexInt

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
    TFLiteMicroHelper,
)

CODEOWNERS = ["@nliaudat"]

DOMAIN = "tflite_micro_runtime"

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

# Re-exported for consumers (meter_reader_tflite, micro_wake_word) which
# reference the runtime class through this component.
__all__ = [
    "TFLiteMicroHelper",
    "CONFIG_SCHEMA",
    "MULTI_CONF",
    "to_code",
]


async def to_code(config):
    if CORE.target_platform == "esp32":
        cg.add_define("USE_TFLITE_MICRO_HELPER")
        model_type = config[CONF_MODEL_TYPE]
        model_path, model_data = resolve_model_source(config)
        # Compile-time operator support check: fail config generation (before
        # compiling/flashing) when the model requires an operator the TFLM
        # operator table cannot register. The device-side check remains the
        # final safety net when no .txt report is available.
        # The op-table lives in the tflite_op_processor sub-component; its
        # module may not be importable here when external_components use a
        # `components:` filter that omits it. Import lazily and degrade to the
        # runtime check in ModelLoader instead of failing config generation.
        try:
            from esphome.components.tflite_op_processor.op_manifest import (
                extract_model_ops,
                parse_op_table,
            )
        except ImportError:
            # Operator table not reachable at config-generation time; rely on
            # the device-side operator check (ModelLoader) as safety net.
            pass
        else:
            model_ops = extract_model_ops(model_path)
            if model_ops:
                supported_ops = parse_op_table().supported
                unsupported_ops = sorted(model_ops - supported_ops)
                if unsupported_ops:
                    raise cv.Invalid(
                        f"Model '{Path(model_path).name}' requires operator(s) "
                        f"{', '.join(unsupported_ops)} which esp-tflite-micro 1.3.7 "
                        f"does not support.\n"
                        f"  Supported operators: {', '.join(sorted(supported_ops))}\n"
                        f"  Re-export the model without the unsupported operator(s) "
                        f"or choose a compatible model."
                    )
        var = cg.new_Pvariable(config[CONF_ID])
        rhs = [HexInt(x) for x in model_data]
        prog_arr = cg.progmem_array(config[CONF_RAW_DATA_ID], rhs)
        cg.add(var.set_model(prog_arr, len(model_data)))
        # Per-instance CRC32 - MULTI_CONF safe, each TFLiteMicroHelper carries its own expected checksum
        crc32_val = zlib.crc32(model_data) & 0xFFFFFFFF
        cg.add(var.set_expected_crc32(crc32_val))
        cg.add(var.set_model_type(model_type))
        cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
        cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
        cg.add_build_flag("-DESP_NN")
        cg.add_build_flag("-DOPTIMIZED_KERNEL=esp_nn")
        # TFLITE_MICRO_MAX_OPERATORS is a process-global compile-time macro.
        # With MULTI_CONF, emitting one value per instance would let the last one
        # win. Compute the max across ALL configured models so every instance is
        # safe, and emit the define only once for the whole build.
        _add_max_operators_define()
        if config.get(CONF_DEBUG, False):
            cg.add_define("DEBUG_TFLITE_MICRO_HELPER")
            cg.add(var.set_debug(True))
        if model_type == "image":
            await _configure_image_model(config, var, model_path, model_data)
        elif model_type == "audio":
            await _configure_audio_model(config, var, model_path, model_data)
        esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
        esp32.add_idf_component(name="espressif/esp-nn", ref="1.2.3")
        if model_type == "audio":
            esp32.add_idf_component(
                name="esphome/esp-micro-speech-features", ref="1.2.3"
            )


def _add_max_operators_define():
    """Emit TFLITE_MICRO_MAX_OPERATORS once for the whole build.

    ``cg.add_define()`` wraps the value in an expression object that compares by
    identity, so calling it from every to_code() invocation (MULTI_CONF) would
    write the same ``#define`` several times. Skip the call when the define is
    already queued; every invocation computes the same value anyway.
    """
    if any(
        define.name == "TFLITE_MICRO_MAX_OPERATORS" for define in CORE.defines
    ):
        return
    cg.add_define("TFLITE_MICRO_MAX_OPERATORS", _compute_max_operators())


def _compute_max_operators():
    """Compute the single TFLITE_MICRO_MAX_OPERATORS value for the ENTIRE build.

    Defines are process-global. With MULTI_CONF each to_code() invocation would
    otherwise emit its own value and the last one wins -- silently breaking
    earlier models with a too-small resolver.
    All invocations therefore compute the SAME max across every configured model.

    Returns max(ops + 5) across all .txt reports, or 30 (the default) when no
    .txt report provides a count.
    """
    if CORE.config is None:
        return 30
    max_ops = 0
    for entry in CORE.config.get(DOMAIN, []):
        try:
            model_path, _ = resolve_model_source(entry)
        except cv.Invalid:
            continue
        txt_path = str(Path(model_path).with_suffix(".txt"))
        if not Path(txt_path).exists():
            continue
        with Path(txt_path).open(encoding="utf-8") as f:
            txt_content = f.read()
        ops_match = re.search(r"Total operations:\s+(\d+)", txt_content)
        if ops_match:
            max_ops = max(max_ops, int(ops_match.group(1)) + 5)
    return max_ops if max_ops > 0 else 30


async def _configure_image_model(entry, var, model_path, model_data):
    model_filename = Path(str(model_path).replace("\\", "/")).name
    auto_config = parse_model_txt_file(model_path)
    if auto_config:
        print(f"  Auto-detected image config from '{model_filename}.txt':")
        for k, v in auto_config.items():
            print(f"    {k}: {v}")
    else:
        auto_config = infer_model_config_from_filename(model_filename)
        print(
            f"  No .txt file found, using filename heuristics for '{model_filename}':"
        )
        for k, v in auto_config.items():
            print(f"    {k}: {v}")
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
    cg.add(var.set_input_type(auto_config.get("input_type", "uint8")))
    cg.add(var.set_input_channels(auto_config.get("input_channels", 3)))
    cg.add(var.set_input_width(auto_config.get("input_width", 32)))
    cg.add(var.set_input_height(auto_config.get("input_height", 20)))
    cg.add(
        var.set_output_processing(auto_config.get("output_processing", "direct_class"))
    )
    cg.add(var.set_scale_factor(auto_config.get("scale_factor", 1.0)))
    cg.add(var.set_input_order(auto_config.get("input_order", "RGB")))
    cg.add(var.set_normalize(auto_config.get("normalize", False)))
    cg.add(var.set_invert(auto_config.get("invert", False)))
    if CONF_TENSOR_ARENA_SIZE in entry:
        cg.add(var.set_tensor_arena_size(entry[CONF_TENSOR_ARENA_SIZE]))
    elif "tensor_arena_size" in auto_config:
        cg.add(var.set_tensor_arena_size(auto_config["tensor_arena_size"]))


async def _configure_audio_model(entry, var, model_path, model_data):
    cg.add_define("USE_TFLITE_STREAMING")
    auto_config = _parse_audio_config(model_path) or {}
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
