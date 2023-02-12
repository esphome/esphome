import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from . import IEC62056Component, CONF_IEC62056_ID, CONF_OBIS, iec62056_ns, validate_obis

IEC62056Sensor = iec62056_ns.class_("IEC62056Sensor", sensor.Sensor)

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        IEC62056Sensor,
    ).extend(
        {
            cv.GenerateID(CONF_IEC62056_ID): cv.use_id(IEC62056Component),
            cv.Required(CONF_OBIS): validate_obis,
        }
    ),
    cv.has_exactly_one_key(CONF_OBIS),
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_IEC62056_ID])
    var = await sensor.new_sensor(config)

    if CONF_OBIS in config:
        cg.add(var.set_obis(config[CONF_OBIS]))

    cg.add(component.register_sensor(var))
