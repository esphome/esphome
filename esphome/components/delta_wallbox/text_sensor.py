import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.components.const import CONF_ERROR_CODE, CONF_SERIAL_NUMBER
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import CONF_DELTA_WALLBOX_ID, DeltaWallbox

DEPENDENCIES = ["delta_wallbox"]

CONF_FW_VERSION = "fw_version"
CONF_BT_FW_VERSION = "bt_fw_version"
CONF_MODEL_NAME = "model_name"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DELTA_WALLBOX_ID): cv.use_id(DeltaWallbox),
        cv.Optional(CONF_SERIAL_NUMBER): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_FW_VERSION): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_BT_FW_VERSION): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_MODEL_NAME): text_sensor.text_sensor_schema(),
        cv.Optional(CONF_ERROR_CODE): text_sensor.text_sensor_schema(),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_DELTA_WALLBOX_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_SERIAL_NUMBER, hub.set_serial_number_text_sensor)
    await text_sensors(CONF_FW_VERSION, hub.set_fw_version_text_sensor)
    await text_sensors(CONF_BT_FW_VERSION, hub.set_bt_fw_version_text_sensor)
    await text_sensors(CONF_MODEL_NAME, hub.set_model_name_text_sensor)
    await text_sensors(CONF_ERROR_CODE, hub.set_error_code_text_sensor)
