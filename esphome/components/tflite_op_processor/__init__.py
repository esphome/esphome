"""Operator resolution for TFLite Micro models.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; its C++ sources are compiled into
the firmware and used by the model handler to register the operators
required by a model.
"""

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
DOMAIN = "tflite_op_processor"

# Library-only component: the empty schema lets the component be loaded (and
# tested) on its own, while tflite_micro_runtime is the user-facing entry point.
CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    # Emit the build flags this component's sources need, so it also works when
    # it is loaded directly instead of through tflite_micro_runtime.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    # The TFLM_OP_CONDITIONAL operators in tflm_operators.h need 1.3.7 or newer.
    # Keep this pin in sync with micro_wake_word and esphome/idf_component.yml:
    # add_idf_component() keeps the last ref it is given, so a different pin
    # elsewhere in the config would make the version depend on load order.
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.3.7")
