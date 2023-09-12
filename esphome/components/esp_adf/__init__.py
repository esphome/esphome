import os

import esphome.config_validation as cv
import esphome.codegen as cg

from esphome.components import esp32

from esphome.const import CONF_ID, CONF_BOARD

CONFLICTS_WITH = ["i2s_audio"]
CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]

CONF_ESP_ADF_ID = "esp_adf_id"

esp_adf_ns = cg.esphome_ns.namespace("esp_adf")
ESPADF = esp_adf_ns.class_("ESPADF", cg.Component)
ESPADFPipeline = esp_adf_ns.class_("ESPADFPipeline", cg.Parented.template(ESPADF))

SUPPORTED_BOARDS = {
    "esp32s3box": "CONFIG_ESP32_S3_BOX_BOARD",
    "esp32s3boxlite": "CONFIG_ESP32_S3_BOX_LITE_BOARD",
}


def _validate_board(config):
    board = config.get(CONF_BOARD, esp32.get_board())
    if board not in SUPPORTED_BOARDS:
        raise cv.Invalid(f"Board {board} is not supported by esp-adf")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESPADF),
            cv.Optional(CONF_BOARD): cv.string_strict,
        }
    ),
    _validate_board,
    cv.only_with_esp_idf,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_define("USE_ESP_ADF")

    cg.add_platformio_option("build_unflags", "-Wl,--end-group")

    esp32.add_idf_component(
        name="esp-adf",
        repo="https://github.com/espressif/esp-adf",
        path="components",
        ref="v2.5",
        components=["*"],
        submodules=["components/esp-sr", "components/esp-adf-libs"],
    )

    cg.add_platformio_option(
        "board_build.embed_txtfiles", "components/dueros_service/duer_profile"
    )
    board = config.get(CONF_BOARD, esp32.get_board())
    esp32.add_idf_sdkconfig_option(SUPPORTED_BOARDS[board], True)

    esp32.add_extra_script(
        "pre",
        "apply_adf_patches.py",
        os.path.join(os.path.dirname(__file__), "apply_adf_patches.py.script"),
    )
    esp32.add_extra_build_file(
        "esp_adf_patches/idf_v4.4_freertos.patch",
        "https://github.com/espressif/esp-adf/raw/v2.5/idf_patches/idf_v4.4_freertos.patch",
    )
