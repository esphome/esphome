import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, simpleevse
from esphome.const import CONF_ID, CONF_DEVICE_CLASS
from . import simpleevse_ns, CONF_SIMPLEEVSE_ID

DEPENDENCIES = ["simpleevse"]
AUTO_LOAD = ["binary_sensor"]

CONF_CONNECTED = "connected"

SimpleEvseSensors = simpleevse_ns.class_("SimpleEvseBinarySensors")

# Schema for sensors
connected_schema = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.Optional(
            CONF_DEVICE_CLASS, default="connectivity"
        ): binary_sensor.device_class,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SimpleEvseSensors),
        cv.GenerateID(CONF_SIMPLEEVSE_ID): cv.use_id(simpleevse.SimpleEvseComponent),
        cv.Optional(CONF_CONNECTED): connected_schema,
    }
)


def to_code(config):
    evse = yield cg.get_variable(config[CONF_SIMPLEEVSE_ID])
    var = cg.new_Pvariable(config[CONF_ID], evse)

    if CONF_CONNECTED in config:
        sens = yield binary_sensor.new_binary_sensor(config[CONF_CONNECTED])
        cg.add(var.set_connected_sensor(sens))
