from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID

from ..mbus import CONF_MBUS_ID, MBus

CODEOWNERS = ["@MarkusSchneider"]
DEPENDENCIES = ["mbus", "sensor"]

mbus_sensor_ns = cg.esphome_ns.namespace("mbus_sensor")
MBusSensor = mbus_sensor_ns.class_("MBusSensor", cg.Component, sensor.Sensor)

CONF_DATA_INDEX = "data_index"
CONF_FACTOR = "factor"

CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(MBusSensor)
    .extend(cv.COMPONENT_SCHEMA)
    .extend(
        {
            cv.GenerateID(CONF_MBUS_ID): cv.use_id(MBus),
            cv.Required(CONF_DATA_INDEX): cv.positive_int,
            cv.Optional(CONF_FACTOR, default=0.0): cv.positive_float,
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_DATA_INDEX],
        config[CONF_FACTOR],
    )
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    parent = await cg.get_variable(config[CONF_MBUS_ID])
    cg.add(parent.add_sensor(var))
