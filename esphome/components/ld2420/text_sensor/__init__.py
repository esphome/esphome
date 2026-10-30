import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP
from esphome.types import ConfigType

from .. import CONF_LD2420_ID, LD2420Component, ld2420_ns, register_listener

LD2420TextSensor = ld2420_ns.class_("LD2420TextSensor", text_sensor.TextSensor)

CONF_FW_VERSION = "fw_version"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LD2420TextSensor),
            cv.GenerateID(CONF_LD2420_ID): cv.use_id(LD2420Component),
            cv.Optional(CONF_FW_VERSION): text_sensor.text_sensor_schema(
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
            ),
        }
    ),
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    text_sensors = text_sensor.sub_text_sensors(config)
    await text_sensors(CONF_FW_VERSION, var.set_fw_version_text_sensor)
    hub = await cg.get_variable(config[CONF_LD2420_ID])
    await register_listener(hub, var)
