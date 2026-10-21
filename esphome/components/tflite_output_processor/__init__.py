"""Output processing for TFLite Micro model results.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; converts raw model output tensors
into (value, confidence) pairs using the configured processing mode
(softmax, argmax, logits, auto_detect, ...).  The caller passes the mode
and scale factor, so this component does not depend on the model loader.
"""

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
DOMAIN = "tflite_output_processor"

# Library-only component: the empty schema lets the component be loaded (and
# tested) on its own, while tflite_micro_runtime is the user-facing entry point.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    # Emit the build flags this component's sources need, so it also works when
    # it is loaded directly instead of through tflite_micro_runtime.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
