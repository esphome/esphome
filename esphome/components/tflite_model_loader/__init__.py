"""Model loading for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; loads .tflite model data into a TFLite
Micro interpreter (operators + tensor arena) and provides CRC32
verification of model data.
"""

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
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
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
