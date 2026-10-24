"""Debug utilities for TFLite Micro models.

Loaded as a dependency of tflite_micro_runtime; provides the diagnostics that
tflite_model_loader declares on ModelHandler (tensor inspection, quantization
analysis, parameter sweeping) plus the DEBUG_TFLITE_MICRO_HELPER verbose paths
used by the rest of the series.
"""

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32", "tflite_model_loader"]
DOMAIN = "tflite_model_debug"

# Library-only component: the empty schema lets the component be loaded (and
# tested) on its own, while tflite_micro_runtime is the user-facing entry point.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config: ConfigType) -> None:
    # Emit the build flags this component's sources need, so it also works when
    # it is loaded directly instead of through tflite_micro_runtime.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    # USE_TFLITE_MODEL_DEBUG is what tflite_model_loader tests: it declares the
    # debug members on ModelHandler, and this component defines them. Both use
    # the same flag, so the class never promises a method this component does not
    # provide -- a build without it has no debug API at all.
    cg.add_define("USE_TFLITE_MODEL_DEBUG")
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
