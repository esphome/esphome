import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_SENSOR_DATAPOINT

from .. import CONF_ECONET_ID, ECONET_CLIENT_SCHEMA, EconetClient, econet_ns

DEPENDENCIES = ["econet"]

EconetSensor = econet_ns.class_(
    "EconetSensor", sensor.Sensor, cg.Component, EconetClient
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(EconetSensor)
    .extend(
        {
            cv.Required(CONF_SENSOR_DATAPOINT): cv.string,
        }
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    paren = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet_parent(paren))

    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))
