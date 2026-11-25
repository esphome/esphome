"""Component to manage TFLite Micro models and inference runtime.

Loads a model, verifies it, and runs inference on a buffer the consumer fills. The
input and output pipelines for a model kind (image, audio, ...) are added by the
components that implement them.
"""

import logging
from pathlib import Path
import zlib

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32
from esphome.components.tflite_model_io.model_reader import (
    estimate_tensor_arena,
    read_model_io,
)
from esphome.components.tflite_model_loader import emit_model_buffer
from esphome.components.tflite_model_source import resolve_model_source
from esphome.components.tflite_op_processor import (
    add_model_operator_defines,
    validate_model_operators,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_ID
from esphome.core import CORE, EsphomeError
from esphome.types import ConfigType

from . import schema as _schema
from .schema import (
    CONF_ON_RESULT,
    CONF_PIPELINE_ID,
    CONF_TENSOR_ARENA_SIZE,
    PER_MODEL_SCHEMA,
    TFLiteMicroHelper,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@nliaudat"]

DOMAIN = "tflite_micro_runtime"

# The pipeline and decoder paths hold the tensors in fixed arrays; keep this in
# sync with MAX_INPUTS/MAX_OUTPUTS in tflite_micro_runtime.cpp.
MAX_MODEL_TENSORS = 8

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
    "CONF_PIPELINE_ID",
    "CONF_TENSOR_ARENA_SIZE",
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
    prog_arr = emit_model_buffer(config, model_data)
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
    add_model_operator_defines(_configured_model_paths())
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


def _validate_model_tensor_counts(model_path: Path) -> None:
    """Reject a model with more tensors than the pipelines can hold."""
    try:
        model_io = read_model_io(model_path)
    except ValueError as err:
        raise cv.Invalid(str(err)) from err
    if (
        len(model_io.inputs) > MAX_MODEL_TENSORS
        or len(model_io.outputs) > MAX_MODEL_TENSORS
    ):
        raise cv.Invalid(
            f"Model '{_model_filename(model_path)}' declares {len(model_io.inputs)} "
            f"inputs and {len(model_io.outputs)} outputs; the runtime supports at "
            f"most {MAX_MODEL_TENSORS} of each"
        )


def final_validate(config: ConfigType) -> None:
    """Check the model itself once every component has validated.

    A model kind points its own ``FINAL_VALIDATE_SCHEMA`` here, so the operator check
    also runs for kind entries, which do not go through this component's schema.
    """
    model_path, _ = resolve_model_source(config)
    validate_model_operators(model_path)
    _validate_model_tensor_counts(model_path)


FINAL_VALIDATE_SCHEMA = final_validate


def _add_build_defines() -> None:
    """Emit the defines that describe the build rather than one model."""
    cg.add_define("USE_TFLITE_MICRO_HELPER")


def _configured_model_paths() -> list[Path]:
    """The model each configured runtime or kind entry uses."""
    if CORE.config is None:
        return []
    return [resolve_model_source(entry)[0] for entry in _model_entries(CORE.config)]


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
