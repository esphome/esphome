"""Component to manage TFLite Micro models and inference runtime.

Loads a model, verifies it, and runs inference on a buffer the consumer fills. The
input and output pipelines for a model kind (image, audio, ...) are added by the
components that implement them.
"""

from dataclasses import dataclass
import inspect
import logging
from pathlib import Path
import zlib

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.tflite_model_io.model_reader import estimate_tensor_arena
from esphome.components.tflite_model_source import resolve_model_source
from esphome.components.tflite_op_processor import (
    emit_all_ops,
    emit_selective_op_defines,
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
    CONF_ON_RESULT,
    CONF_TENSOR_ARENA_SIZE,
    PER_MODEL_SCHEMA,
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
    "tflite_model_io",
    "tflite_model_loader",
    "tflite_model_source",
    "tflite_op_processor",
    "tflite_output_processor",
]

# Attributes consumed by the ESPHome config loader.
CONFIG_SCHEMA = _schema.CONFIG_SCHEMA
MULTI_CONF = _schema.MULTI_CONF
# A model kind auto-loads this component without a configuration entry of its own.
MULTI_CONF_NO_DEFAULT = True
DEPENDENCIES = _schema.DEPENDENCIES + ["tflite_model_source"]

# Re-exported for consumers which reference the runtime class through this component.
__all__ = [
    "TFLiteMicroHelper",
    "CONFIG_SCHEMA",
    "FINAL_VALIDATE_SCHEMA",
    "MULTI_CONF",
    "PER_MODEL_SCHEMA",
    "declare_model",
    "to_code",
]


async def declare_model(config: ConfigType) -> tuple[cg.MockObj, Path]:
    """Create and configure the runtime helper for one model.

    A model kind calls this from its own ``to_code()``, so the kind is the
    configuration entry and this component is loaded in the background.

    @return The helper and the resolved model path, for the kind's own setup.
    """
    _add_build_defines()
    model_path, model_data = resolve_model_source(config)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    prog_arr = _emit_model_buffer(config, model_data)
    # Per-instance CRC32 - MULTI_CONF safe, each helper carries its own checksum.
    cg.add(
        var.set_model(prog_arr, len(model_data), zlib.crc32(model_data) & 0xFFFFFFFF)
    )
    for conf in config.get(CONF_ON_RESULT, []):
        # Queued, not awaited: a lambda may reference the kind's decoder, which the
        # kind declares after this function returns.
        CORE.add_job(
            automation.build_callback_automation,
            var,
            "add_on_result_callback",
            [],
            conf,
        )
    cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
    cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
    cg.add_build_flag("-DESP_NN")
    cg.add_build_flag("-DOPTIMIZED_KERNEL=esp_nn")
    # Operator defines are process-global, so they cover ALL configured models.
    _add_operator_defines()
    if config.get(CONF_DEBUG, False):
        cg.add(var.set_debug(True))
    if (arena_size := config.get(CONF_TENSOR_ARENA_SIZE)) is not None:
        cg.add(var.set_tensor_arena_size(arena_size))
    else:
        # A seed only: the device measures what the model needs and allocates that.
        try:
            estimate = estimate_tensor_arena(model_path)
        except ValueError as err:
            raise EsphomeError(
                f"Could not estimate tensor_arena_size for '{_model_filename(model_path)}': "
                f"{err}. Set tensor_arena_size explicitly to bypass estimation."
            ) from err
        _LOGGER.info(
            "Estimated tensor arena for '%s': %d bytes (%d peak activation bytes "
            "over %d tensors)",
            _model_filename(model_path),
            estimate.bytes,
            estimate.peak_activation_bytes,
            estimate.tensor_count,
        )
        cg.add(var.set_tensor_arena_size(estimate.bytes))
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.4.1")
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.5.0")
    return var, model_path


async def to_code(config: ConfigType) -> None:
    await declare_model(config)


def _model_filename(model_path: Path) -> str:
    """File name of the resolved model, with Windows separators normalised."""
    return Path(str(model_path).replace("\\", "/")).name


def _progmem_array_supports_align() -> bool:
    """Whether ``progmem_array()`` takes ``align`` (added in 2026.12, #22620)."""
    return "align" in inspect.signature(cg.progmem_array).parameters


_PROGMEM_ARRAY_SUPPORTS_ALIGN = _progmem_array_supports_align()


def _emit_model_buffer(config: ConfigType, model_data: bytes) -> cg.MockObj:
    """Emit the model as a 16-byte aligned PROGMEM array and return a reference."""
    # TFLM casts the buffer to a flatbuffer, so it cannot be byte-aligned.
    values = [HexInt(x) for x in model_data]
    if _PROGMEM_ARRAY_SUPPORTS_ALIGN:
        return cg.progmem_array(config[CONF_RAW_DATA_ID], values, align=16)
    # ESPHome 2026.11 and earlier: ``align`` does not exist yet. Emit the same
    # declaration the updated helper produces, so the buffer keeps its alignment.
    id_ = config[CONF_RAW_DATA_ID]
    cg.add_global(
        cg.RawExpression(
            f"alignas(16) static constexpr {id_.type} {id_}[] PROGMEM = "
            f"{cg.safe_exp(values)}"
        )
    )
    obj = cg.MockObj(id_, ".")
    CORE.register_variable(id_, obj)
    return obj


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


def final_validate(config: ConfigType) -> None:
    """Check the model itself once every component has validated.

    A model kind points its own ``FINAL_VALIDATE_SCHEMA`` here, so the operator check
    also runs for kind entries, which do not go through this component's schema.
    """
    model_path, _ = resolve_model_source(config)
    _validate_model_operators(model_path)


FINAL_VALIDATE_SCHEMA = final_validate


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
        for entry in _model_entries(CORE.config)
    ]


def _is_runtime_model_entry(entry: object) -> bool:
    """True for an entry whose id is a TFLiteMicroHelper.

    Other components (displays, gas sensors) also have a ``model`` key, so the key
    names alone would send their hardware model strings to the source resolver.
    """
    if not isinstance(entry, dict):
        return False
    model_type = getattr(entry.get(CONF_ID), "type", None)
    return isinstance(model_type, cg.MockObjClass) and model_type.inherits_from(
        TFLiteMicroHelper
    )


def _model_entries(config: ConfigType) -> list[ConfigType]:
    """Every model entry in the configuration, whichever component owns it.

    A model kind is the entry of its own component, so the operators cannot be
    gathered from this component's key alone.
    """
    entries: list[ConfigType] = []
    for value in config.values():
        candidates = value if isinstance(value, list) else [value]
        entries.extend(entry for entry in candidates if _is_runtime_model_entry(entry))
    return entries


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
