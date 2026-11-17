import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_SENSOR_DATAPOINT

from .. import CONF_TUYA_ID, Tuya, register_tuya_child, tuya_ns

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@jesserockz"]

TuyaBinarySensor = tuya_ns.class_("TuyaBinarySensor", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(TuyaBinarySensor).extend(
    {
        cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
        cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)

    hub = await cg.add_variable(var.set_tuya_parent, config[CONF_TUYA_ID])

    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))
    register_tuya_child(hub, var)
