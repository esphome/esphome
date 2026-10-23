"""Tensor arena memory management for TFLite Micro models.

Loaded as a dependency of tflite_micro_runtime; provides PSRAM-aware tensor
arena allocation (MemoryManager) and arena size probing. Only the heap the
arena is taken from is configurable::

    tflite_memory_handler:
      arena_location: auto  # or internal / psram
"""

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32", "tflite_op_processor"]
DOMAIN = "tflite_memory_handler"

CONF_ARENA_LOCATION = "arena_location"

# Where the tensor arena is allocated. "auto" uses PSRAM when the board has it and
# internal RAM otherwise, never crossing over: an arena that falls back to the
# small internal heap exhausts it and crashes somewhere unrelated later.
ARENA_LOCATIONS = {
    "auto": None,
    "internal": "TFLITE_ARENA_INTERNAL",
    "psram": "TFLITE_ARENA_PSRAM",
}

# Library-only component otherwise: the empty mapping keeps the component loadable
# on its own (and by the component tests), while tflite_micro_runtime is the
# user-facing entry point.
CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ARENA_LOCATION, default="auto"): cv.one_of(
            *ARENA_LOCATIONS, lower=True
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    # Emit the build flags this component's sources need, so it also works when
    # it is loaded directly instead of through tflite_micro_runtime.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    if (define := ARENA_LOCATIONS[config[CONF_ARENA_LOCATION]]) is not None:
        cg.add_define(define)
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
    # esp-nn decides how much memory each kernel needs, so a probed arena size
    # must not depend on which version another component pinned.
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.2.3")
