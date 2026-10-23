import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC
from esphome.types import ConfigType

from . import (
    CONF_MODEM_ID,
    SIM7600_FAMILY,
    ModemComponent,
    request_polling,
    require_model,
)

DEPENDENCIES = ["modem"]

CONF_NETWORK_TYPE = "network_type"
CONF_URC = "urc"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MODEM_ID): cv.use_id(ModemComponent),
        cv.Optional(CONF_NETWORK_TYPE): text_sensor.text_sensor_schema(
            icon="mdi:signal-cellular-3", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_URC): text_sensor.text_sensor_schema(),
    }
)

# The network type comes from AT+CNSMOD, decoded with the SIM7600 table
FINAL_VALIDATE_SCHEMA = require_model((CONF_NETWORK_TYPE,), SIM7600_FAMILY)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MODEM_ID])
    texts = text_sensor.sub_text_sensors(config)
    if await texts(CONF_URC, hub.set_urc_text_sensor) is not None:
        cg.add_define("USE_MODEM_URC")
        cg.add_define("USE_LINE_READER")
        add_idf_sdkconfig_option("CONFIG_ESP_MODEM_URC_HANDLER", True)
    if await texts(CONF_NETWORK_TYPE, hub.set_network_type_text_sensor) is not None:
        cg.add_define("USE_MODEM_NETWORK_TYPE")
        request_polling(hub)
