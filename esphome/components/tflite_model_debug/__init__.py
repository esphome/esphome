"""Debug utilities for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; provides the DEBUG_TFLITE_MICRO_HELPER
diagnostics (tensor inspection, quantization analysis, parameter sweeping)
defined out-of-line on ModelHandler.
"""

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32", "tflite_model_loader"]
DOMAIN = "tflite_model_debug"

# Library-only component: the empty schema lets the component be loaded (and
# tested) on its own, while tflite_micro_runtime is the user-facing entry point.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    # Emit the build flags this component's sources need, so it also works when
    # it is loaded directly instead of through tflite_micro_runtime.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
