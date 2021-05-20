import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, ICON_FLASH, UNIT_WATT_HOURS

from .. import teleinfo_ns, TeleInfo, CONF_TELEINFO_ID

CONF_TAG_NAME = "tag_name"

TeleInfoSensor = teleinfo_ns.class_("TeleInfoSensor", sensor.Sensor, cg.Component)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_WATT_HOURS, ICON_FLASH, 0).extend(
    {
        cv.GenerateID(): cv.declare_id(TeleInfoSensor),
        cv.GenerateID(CONF_TELEINFO_ID): cv.use_id(TeleInfo),
        cv.Required(CONF_TAG_NAME): cv.string,
    }
)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
    teleinfo = yield cg.get_variable(config[CONF_TELEINFO_ID])
    cg.add(teleinfo.register_teleinfo_listener(var))
