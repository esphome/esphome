import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG
from esphome.core import CORE

DEPENDENCIES = ["esp32"]
MULTI_CONF = True

tflite_micro_helper_ns = cg.esphome_ns.namespace("tflite_micro_helper")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DEBUG, default=False): cv.boolean,
    }
)


async def to_code(config):
    cg.add_define("USE_TFLITE_MICRO_HELPER")

    if CORE.target_platform == "esp32":
        esp32.add_idf_component(
            name="espressif/esp-tflite-micro",
            ref="1.3.7",
        )

        esp32.add_idf_component(
            name="espressif/esp-nn",
            ref="1.2.3",
        )

        cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
        cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
        cg.add_build_flag("-DESP_NN")
        cg.add_build_flag("-DOPTIMIZED_KERNEL=esp_nn")

    if config.get(CONF_DEBUG, False):
        cg.add_define("DEBUG_TFLITE_MICRO_HELPER")