import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE_ID

from .. import Number, number_ns

NumberSensor = number_ns.class_("NumberSensor", sensor.Sensor, cg.Component)


CONFIG_SCHEMA = (
    sensor.sensor_schema(NumberSensor)
    .extend(
        {
            cv.Required(CONF_SOURCE_ID): cv.use_id(Number),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    source = await cg.get_variable(config[CONF_SOURCE_ID])
    var = await sensor.new_sensor(config, source)
    await cg.register_component(var, config)
