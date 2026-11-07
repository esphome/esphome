"""Output processing for TFLite Micro model results.

Library-only component: no user-facing configuration. Loaded as a
dependency of tflite_micro_runtime; converts raw model output tensors
into (value, confidence) pairs using the configured processing mode
(argmax, softmax, qat_quantized or auto_detect).  The caller passes the
mode and scale factor, so this component does not depend on the model loader.

It also exports the OutputProcessing enum plus OUTPUT_PROCESSING, so the
caller validates the mode with cv.enum and passes the enum value instead of
comparing strings on every inference.
"""

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
DOMAIN = "tflite_output_processor"

output_processor_ns = cg.esphome_ns.namespace("tflite_output_processor")

OutputProcessing = output_processor_ns.enum("OutputProcessing", is_class=True)
OUTPUT_PROCESSING = OutputProcessing.options(
    "argmax",
    "softmax",
    "qat_quantized",
    "auto_detect",
    prefix="OUTPUT_PROCESSING_",
)

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
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.4.2")
