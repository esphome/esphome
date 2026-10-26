import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.esp32 import add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_MODEL, ENTITY_CATEGORY_DIAGNOSTIC
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_MODEM_ID, ModemComponent, request_polling

DEPENDENCIES = ["modem"]

CONF_NETWORK_TYPE = "network_type"
CONF_URC = "urc"

# Models whose AT+CNSMOD answers match the table the network type is decoded with
NETWORK_TYPE_MODELS = ("SIM7600", "SIM7670")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MODEM_ID): cv.use_id(ModemComponent),
        cv.Optional(CONF_NETWORK_TYPE): text_sensor.text_sensor_schema(
            icon="mdi:signal-cellular-3", entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional(CONF_URC): text_sensor.text_sensor_schema(),
    }
)


def _validate_network_type_model(hub: ConfigType) -> ConfigType:
    if (model := hub[CONF_MODEL]) not in NETWORK_TYPE_MODELS:
        raise cv.Invalid(
            f"{CONF_NETWORK_TYPE} is not supported on {model}; supported models: {', '.join(NETWORK_TYPE_MODELS)}",
            path=[CONF_MODEL],
        )
    return hub


def _final_validate(config: ConfigType) -> ConfigType:
    if CONF_NETWORK_TYPE in config:
        fv.id_declaration_match_schema(_validate_network_type_model)(
            config[CONF_MODEM_ID]
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


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
