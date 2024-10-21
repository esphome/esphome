import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import microphone
from esphome.const import CONF_ID

from .. import (
    CONF_ESP_ADF_ID,
    ESPADF,
    ESPADFPipeline,
    esp_adf_ns,
    final_validate_usable_board,
)

AUTO_LOAD = ["esp_adf"]
CONFLICTS_WITH = ["i2s_audio"]
DEPENDENCIES = ["esp32"]

ESPADFMicrophone = esp_adf_ns.class_(
    "ESPADFMicrophone", ESPADFPipeline, microphone.Microphone, cg.Component
)


CONFIG_SCHEMA = cv.All(
    microphone.MICROPHONE_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(ESPADFMicrophone),
            cv.GenerateID(CONF_ESP_ADF_ID): cv.use_id(ESPADF),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_esp_idf,
)

FINAL_VALIDATE_SCHEMA = final_validate_usable_board("microphone")


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_ESP_ADF_ID])

    await microphone.register_microphone(var, config)
