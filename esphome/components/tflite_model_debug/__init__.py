"""Debug utilities for TFLite Micro models.

Loaded as a dependency of tflite_micro_runtime; provides the diagnostics that
tflite_model_loader declares on ModelHandler (tensor inspection, quantization
analysis, parameter sweeping). The definitions are guarded by
USE_TFLITE_MODEL_DEBUG, which this component's to_code() emits when a model
enables debug.
"""

import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_MODEL
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32", "tflite_model_loader"]
DOMAIN = "tflite_model_debug"

# Library-only component: the empty schema lets the component be loaded (and
# tested) on its own, while tflite_micro_runtime is the user-facing entry point.
CONFIG_SCHEMA = cv.Schema({})


def _model_debug_requested() -> bool:
    """Whether any configured model enables the diagnostics.

    A model kind is its own entry, so the whole configuration is scanned rather
    than this component's (empty) block.
    """
    if CORE.config is None:
        return False
    for value in CORE.config.values():
        entries = value if isinstance(value, list) else [value]
        for entry in entries:
            if (
                isinstance(entry, dict)
                and entry.get(CONF_DEBUG) is True
                and CONF_MODEL in entry
            ):
                return True
    return False


async def to_code(config: ConfigType) -> None:
    # Emit the build flags this component's sources need, so it also works when
    # it is loaded directly instead of through tflite_micro_runtime.
    cg.add_define("USE_TFLITE_MICRO_HELPER")
    # USE_TFLITE_MODEL_DEBUG is what tflite_model_loader tests: it declares the
    # debug members on ModelHandler, and this component defines them. Both use
    # the same flag, so the class never promises a method this component does not
    # provide -- a build without it has no debug API at all. It stays off unless
    # a model asks for the diagnostics, so their code is not linked otherwise.
    if _model_debug_requested():
        cg.add_define("USE_TFLITE_MODEL_DEBUG")
    esp32.add_idf_component(name="espressif/esp-tflite-micro", ref="1.4.1")
    # esp-nn is pinned with it because it decides how much memory each kernel
    # needs, so an unpinned esp-nn makes the arena sizes vary per build.
    esp32.add_idf_component(name="espressif/esp-nn", ref="1.5.0")
