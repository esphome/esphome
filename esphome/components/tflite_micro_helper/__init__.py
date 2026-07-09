import esphome.codegen as cg
from esphome.components import esp32
import esphome.config_validation as cv
from esphome.core import CORE

DEPENDENCIES = ["esp32"] if CORE.target_platform == "esp32" else []

tflite_micro_helper_ns = cg.esphome_ns.namespace("tflite_micro_helper")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Optional("debug", default=False): cv.boolean,
    }
)


async def to_code(config):
    cg.add_define("USE_TFLITE_MICRO_HELPER")

    if CORE.target_platform == "esp32":
        esp32.add_idf_component(
            name="espressif/esp-tflite-micro",
            # ref="~1.3.4" #https://github.com/espressif/esp-tflite-micro/issues/120
            # ref="1.3.4" # fix to 1.3.4 cause 1.3.5 has bug
            ref="1.3.7",
        )

        esp32.add_idf_component(
            name="espressif/esp-nn",
            # ref="~1.1.2"
            # ref="1.2.1"
            ref="1.2.3",
        )

        # Force remove default -std flags and apply correct ones
        # cg.add_platformio_option("build_unflags", ["-std=gnu++11", "-std=gnu99"])
        # cg.add_build_flag("-std=gnu++14")
        # cg.add_build_flag("-std=gnu11")  # For C files

        # Alternative: Just disable -Werror for this component
        # cg.add_build_flag("-Wno-error")

        cg.add_build_flag("-DTF_LITE_STATIC_MEMORY")
        cg.add_build_flag("-DTF_LITE_DISABLE_X86_NEON")
        cg.add_build_flag("-DESP_NN")
        # cg.add_build_flag("-DUSE_ESP32_CAMERA_CONV")
        cg.add_build_flag("-DOPTIMIZED_KERNEL=esp_nn")
    else:
        # On host, we don't link actual TFLite
        pass

    if config.get("debug", False):
        cg.add_define("DEBUG_TFLITE_MICRO_HELPER")
