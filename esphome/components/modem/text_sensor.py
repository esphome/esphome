import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_MODEM_ID, ModemComponent

DEPENDENCIES = ["modem"]

CONF_URC = "urc"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MODEM_ID): cv.use_id(ModemComponent),
        cv.Optional(CONF_URC): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MODEM_ID])
    texts = text_sensor.sub_text_sensors(config)
    if await texts(CONF_URC, hub.set_urc_text_sensor) is not None:
        cg.add_define("USE_MODEM_URC")
        add_idf_sdkconfig_option("CONFIG_ESP_MODEM_URC_HANDLER", True)
