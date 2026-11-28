"""Model loading for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; loads .tflite model data into a TFLite
Micro interpreter (operators + tensor arena) and provides CRC32
verification of model data.
"""

import inspect

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_RAW_DATA_ID
from esphome.core import CORE, HexInt
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = [
    "esp32",
    "tflite_memory_handler",
    "tflite_op_processor",
    "tflite_output_processor",
]
DOMAIN = "tflite_model_loader"

# Library-only component: the empty schema lets the component be loaded (and
# tested) on its own, while tflite_micro_runtime is the user-facing entry point.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config: ConfigType) -> None:
    # Emit the build flags this component's sources need, so it also works when
    # it is loaded directly instead of through tflite_micro_runtime.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.4.1")
    # esp-nn is pinned with it because it decides how much memory each kernel
    # needs, so an unpinned esp-nn makes the arena sizes vary per build.
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.5.0")


def _progmem_array_supports_align() -> bool:
    """Whether ``progmem_array()`` takes ``align`` (added in 2026.12, #22620)."""
    return "align" in inspect.signature(cg.progmem_array).parameters


_PROGMEM_ARRAY_SUPPORTS_ALIGN = _progmem_array_supports_align()


def emit_model_buffer(config: ConfigType, model_data: bytes) -> cg.MockObj:
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
