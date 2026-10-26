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
from esphome.components.tflite_op_processor import emit_selective_op_defines
from esphome.components.tflite_op_processor.op_manifest import (
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
    OUTPUT_PROCESSING_MODES,
    REPORT_VALUES_SCHEMAS,
    TFLiteMicroHelper,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@nliaudat"]

DOMAIN = "tflite_micro_runtime"

# Operator slots kept when no .txt report says how many the model needs.
_MAX_OPERATORS_FLOOR = 30

# Headroom over the counted operators, which also covers a model that registers one
# slot per operator version.
_MAX_OPERATORS_MARGIN = 5

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
    "FINAL_VALIDATE_SCHEMA",
    "MULTI_CONF",
    "to_code",
]


async def to_code(config: ConfigType) -> None:
    _add_build_defines()
    model_type = config[CONF_MODEL_TYPE]
    model_path, model_data = resolve_model_source(config)
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
        await _configure_image_model(config, var, model_path)
    elif model_type == "audio":
        await _configure_audio_model(config, var, model_path)
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.4.1")
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.4.1")
    if model_type == "audio":
        esp32.add_idf_component(name="esphome/esp-micro-speech-features", ref="1.2.3")


def _model_filename(model_path: Path) -> str:
    """File name of the resolved model, with Windows separators normalised."""
    return Path(str(model_path).replace("\\", "/")).name


def _validate_model_operators(model_path: Path) -> None:
    """Reject a model whose operators the pinned TFLM cannot register.

    A model without a .txt report cannot be checked here; the loader's
    device-side check covers it at load time. A report that names no operator at
    all is treated the same way: it says nothing about what the model runs, so it
    must not pass as a model that needs no kernel.
    """
    model_ops = extract_model_ops(model_path)
    if not model_ops:
        _LOGGER.info(
            "No usable operator report for model '%s'; the operator check runs on the device",
            _model_filename(model_path),
        )
        return
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
    """Validate the values a model's .txt report supplies.

    The report is written by the model exporter, so one unknown value in it would
    otherwise reach code generation, where cv.Invalid is not caught and the user
    sees a traceback instead of a configuration error.
    """
    try:
        return REPORT_VALUES_SCHEMAS[model_type](values)
    except cv.Invalid as err:
        # cv.Invalid of a key loses its key name when it is re-raised as text, so
        # repeat the path here to keep the message pointing at the report entry.
        where = "".join(f"[{key}]" for key in err.path)
        raise cv.Invalid(
            f"Invalid value in {source} of model "
            f"'{_model_filename(model_path)}'{f' {where}' if where else ''}: {err}"
        ) from err


def _report_values(model_path: Path, model_type: str) -> ConfigType | None:
    """The values of the model's .txt report, validated, or None without one.

    Both the final validation and code generation read the report through this
    function, so the values that reach the C++ setters are the ones the schema
    produced: a spelling it lowercases, a size it converts to bytes.
    """
    if (report := _parse_report(model_path, model_type)) is None:
        return None
    return _validate_report_values(report, model_type, model_path)


def _final_validate(config: ConfigType) -> None:
    """Check the model itself once every component has validated.

    Both checks need the resolved model file and both can reject the
    configuration: an operator esp-tflite-micro cannot register, or a .txt report
    value the runtime cannot use. They run here rather than in to_code() so the
    failure names the model and is reported as a configuration error. An image
    model is resolved completely, which also rejects it when no source provides
    its input shape.
    """
    model_type = config[CONF_MODEL_TYPE]
    model_path, _ = resolve_model_source(config)
    _validate_model_operators(model_path)
    if model_type == "image":
        # Resolving the image configuration is also what rejects a model whose input
        # shape no report, file name or YAML option provides.
        _resolve_image_config(config, model_path)
        return
    # The same call code generation reads the report through, so a value that only
    # passes because it is coerced is also the value that reaches the C++ setters.
    _report_values(model_path, model_type)


FINAL_VALIDATE_SCHEMA = _final_validate


def _add_build_defines() -> None:
    """Emit the defines that describe the build rather than one model.

    ``USE_TFLITE_MICRO_HELPER`` compiles the runtime in and
    ``DEBUG_TFLITE_MICRO_HELPER`` compiles in the diagnostics of the loader it
    calls: the check that the buffer starts with the TFL3 identifier, and the arena
    usage it logs when tensor allocation fails. Both are emitted for every build,
    because the second one is read when a model fails to load -- on a device that
    would have had to be built with it beforehand. Neither belongs to a model: a
    model's ``debug:`` only calls set_debug(), which switches the verbose logging
    of that one helper. CORE.defines is a set keyed by name and value, so the
    MULTI_CONF invocations that emit a value-less define again keep one entry.
    """
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    cg.add_define("DEBUG_TFLITE_MICRO_HELPER")


def _add_operator_defines() -> None:
    """Emit the operator defines once for the whole build.

    Unlike the value-less defines above, a define that carries a value keeps one
    entry per call: ``cg.add_define()`` wraps the value in an expression object
    that compares by identity. Calling this from every to_code() invocation
    (MULTI_CONF) would write the same ``#define`` several times, so skip it when it
    is already queued; every invocation computes the same value anyway.
    """
    if any(define.name == "TFLITE_MICRO_MAX_OPERATORS" for define in CORE.defines):
        return
    cg.add_define("TFLITE_MICRO_MAX_OPERATORS", _compute_max_operators())
    if (ops := _selective_op_defines()) is None:
        return
    # tflite_op_processor owns the flags, so every component that configures the
    # selective build widens the same process-global set through one place.
    emit_selective_op_defines(sorted(ops))


def _selective_op_defines() -> set[str] | None:
    """Return the operators the configured models need, or None to keep every kernel.

    The kernels no model uses can be left out only when the operator set of every
    configured model is known at config time. One model without a .txt report, or
    with a report that names no operator, keeps the whole table: a firmware missing
    the kernel that model needs would fail at registration time on the device
    instead of at build time.
    """
    if CORE.config is None:
        return None
    ops: set[str] = set()
    for entry in CORE.config.get(DOMAIN, []):
        model_ops = extract_model_ops(resolve_model_source(entry)[0])
        if not model_ops:
            _LOGGER.info(
                "A configured model has no usable operator report, so every operator is compiled in"
            )
            return None
        ops |= model_ops
    if not ops:
        return None
    return ops


def _compute_max_operators() -> int:
    """Compute the single TFLITE_MICRO_MAX_OPERATORS value for the ENTIRE build.

    Defines are process-global. With MULTI_CONF each to_code() invocation would
    otherwise emit its own value and the last one wins -- silently breaking
    earlier models with a too-small resolver.
    All invocations therefore compute the SAME max across every configured model.

    Each report is read two ways: its operator list gives the operator types the
    resolver registers, its "Total operations" line the number of nodes, which also
    covers a model that registers several versions of one operator. The larger
    reading wins, so a report that has only one of them still sizes the resolver
    instead of leaving it at the floor. The floor itself is not optional: a model
    whose report is missing would otherwise get whatever count the other models
    produced, and its resolver would only fail at registration on the device.
    """
    if CORE.config is None:
        return _MAX_OPERATORS_FLOOR
    max_ops = 0
    for entry in CORE.config.get(DOMAIN, []):
        model_path, _ = resolve_model_source(entry)
        # The operator list is what selective op compilation is built from, so it
        # also sizes the resolver: a report without a "Total operations" line must
        # not leave it at the floor while its operator list enables selective ops.
        model_ops = extract_model_ops(model_path)
        if model_ops is not None:
            max_ops = max(max_ops, len(model_ops) + _MAX_OPERATORS_MARGIN)
        txt_path = Path(model_path).with_suffix(".txt")
        if not txt_path.exists():
            _LOGGER.info(
                "No operator report for model '%s'; keeping %d operator slots",
                _model_filename(model_path),
                _MAX_OPERATORS_FLOOR,
            )
            continue
        txt_content = txt_path.read_text(encoding="utf-8")
        if ops_match := _TOTAL_OPERATIONS_RE.search(txt_content):
            max_ops = max(max_ops, int(ops_match.group(1)) + _MAX_OPERATORS_MARGIN)
        elif model_ops is not None:
            _LOGGER.warning(
                "Operator report '%s' has no 'Total operations' line; sizing the "
                "resolver from its %d listed operator types",
                txt_path.name,
                len(model_ops),
            )
        else:
            _LOGGER.warning(
                "Operator report '%s' has no 'Total operations' line; keeping %d "
                "operator slots",
                txt_path.name,
                _MAX_OPERATORS_FLOOR,
            )
    return max(_MAX_OPERATORS_FLOOR, max_ops)


# Keys a .txt report supplies under the same name as the YAML option, so a value
# given in the YAML wins by overwriting it.
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


def _resolve_image_config(entry: ConfigType, model_path: Path) -> ConfigType:
    """The image configuration, with the YAML values applied over the report.

    The width and height have no default: the file name conventions describe the
    channel order only, so a model with no shape from either source is rejected here
    instead of building a firmware whose load cannot match its input tensor.
    """
    model_filename = _model_filename(model_path)
    if (values := _report_values(model_path, "image")) is not None:
        auto_config = values
    else:
        auto_config = _validate_report_values(
            infer_model_config_from_filename(model_filename),
            "image",
            model_path,
            source="the file name conventions",
        )
    _merge_report_values(auto_config, entry, _IMAGE_REPORT_KEYS)
    if missing := [
        key
        for key in (CONF_INPUT_WIDTH, CONF_INPUT_HEIGHT)
        if auto_config.get(key) is None
    ]:
        raise cv.Invalid(
            f"Model '{model_filename}' has no {' or '.join(missing)}: neither its "
            f".txt report nor the YAML configuration sets "
            f"{'them' if len(missing) > 1 else 'it'}, and the file name conventions "
            f"only describe the channel order. Set {' and '.join(missing)} for the "
            "model."
        )
    return auto_config


async def _configure_image_model(
    entry: ConfigType, var: cg.MockObj, model_path: Path
) -> None:
    auto_config = _resolve_image_config(entry, model_path)
    _LOGGER.info(
        "Image configuration for '%s': %s", _model_filename(model_path), auto_config
    )
    cg.add(var.set_input_type(INPUT_TYPES[auto_config.get(CONF_INPUT_TYPE, "uint8")]))
    cg.add(var.set_input_channels(auto_config.get(CONF_INPUT_CHANNELS, 3)))
    cg.add(var.set_input_width(auto_config[CONF_INPUT_WIDTH]))
    cg.add(var.set_input_height(auto_config[CONF_INPUT_HEIGHT]))
    # Through the table, so C++ only ever receives a mode tflite_output_processor
    # implements -- also for a YAML alias, which cv.enum keeps as the alias name.
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


async def _configure_audio_model(
    entry: ConfigType, var: cg.MockObj, model_path: Path
) -> None:
    report = _report_values(model_path, "audio")
    if report is None:
        # Every audio parameter then comes from YAML or from the C++ defaults, and
        # a streaming model whose window or feature count is wrong still builds.
        _LOGGER.warning(
            "No audio metadata for '%s'; the audio options come from the YAML "
            "configuration or the C++ defaults",
            _model_filename(model_path),
        )
        report = {}
    _merge_report_values(report, entry, _AUDIO_REPORT_KEYS)
    if (arena_size := report.get(CONF_TENSOR_ARENA_SIZE)) is not None:
        cg.add(var.set_tensor_arena_size(arena_size))
    if (cutoff := report.get(CONF_PROBABILITY_CUTOFF)) is not None:
        cg.add(var.set_probability_cutoff(cutoff))
    if (window := report.get(CONF_SLIDING_WINDOW_SIZE)) is not None:
        cg.add(var.set_sliding_window_size(window))
    if (step := report.get(CONF_FEATURES_STEP_SIZE)) is not None:
        cg.add(var.set_features_step_size(step))
    if (features := report.get(CONF_FEATURE_COUNT)) is not None:
        cg.add(var.set_feature_count(features))
