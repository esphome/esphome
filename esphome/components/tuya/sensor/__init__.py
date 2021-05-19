from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@jesserockz"]

CONF_SENSOR_DATAPOINT = "sensor_datapoint"

TuyaSensor = tuya_ns.class_("TuyaSensor", sensor.Sensor, cg.Component)

CONFIG_SCHEMA = sensor.SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TuyaSensor),
        cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
        cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    paren = yield cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))
