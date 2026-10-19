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
    parent = await cg.get_variable(config[CONF_DELTA_WALLBOX_ID])

    if serial_number_config := config.get(CONF_SERIAL_NUMBER):
        sens = await text_sensor.new_text_sensor(serial_number_config)
        cg.add(parent.set_serial_number_sensor(sens))

    if fw_version_config := config.get(CONF_FW_VERSION):
        sens = await text_sensor.new_text_sensor(fw_version_config)
        cg.add(parent.set_fw_version_sensor(sens))

    if bt_fw_version_config := config.get(CONF_BT_FW_VERSION):
        sens = await text_sensor.new_text_sensor(bt_fw_version_config)
        cg.add(parent.set_bt_fw_version_sensor(sens))

    if model_name_config := config.get(CONF_MODEL_NAME):
        sens = await text_sensor.new_text_sensor(model_name_config)
        cg.add(parent.set_model_name_sensor(sens))

    if error_code_config := config.get(CONF_ERROR_CODE):
        sens = await text_sensor.new_text_sensor(error_code_config)
        cg.add(parent.set_error_code_sensor(sens))
