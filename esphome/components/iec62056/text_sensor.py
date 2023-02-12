import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_GROUP
from . import IEC62056Component, CONF_IEC62056_ID, CONF_OBIS, iec62056_ns, validate_obis

AUTO_LOAD = ["iec62056"]

IEC62056TextSensor = iec62056_ns.class_("IEC62056TextSensor", text_sensor.TextSensor)

CONFIG_SCHEMA = cv.All(
    text_sensor.text_sensor_schema(
        IEC62056TextSensor,
    ).extend(
        {
            cv.GenerateID(CONF_IEC62056_ID): cv.use_id(IEC62056Component),
            cv.Required(CONF_OBIS): validate_obis,
            cv.Optional(CONF_GROUP, default=1): cv.int_range(min=0, max=2),
        }
    ),
    cv.has_exactly_one_key(CONF_OBIS),
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_IEC62056_ID])
    var = await text_sensor.new_text_sensor(config)

    if CONF_OBIS in config:
        cg.add(var.set_obis(config[CONF_OBIS]))

    if CONF_GROUP in config:
        cg.add(var.set_group(config[CONF_GROUP]))

    cg.add(component.register_sensor(var))
