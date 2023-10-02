import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_SENSOR_DATAPOINT

from .. import (
    CONF_ECONET_ID,
    CONF_LISTEN_ONLY,
    ECONET_CLIENT_SCHEMA,
    EconetClient,
    econet_ns,
)

DEPENDENCIES = ["econet"]

EconetBinarySensor = econet_ns.class_(
    "EconetBinarySensor", binary_sensor.BinarySensor, cg.Component, EconetClient
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(EconetBinarySensor)
    .extend(
        {
            cv.Required(CONF_SENSOR_DATAPOINT): cv.string,
        }
    )
    .extend(ECONET_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_ECONET_ID])
    cg.add(var.set_econet_parent(paren))
    cg.add(var.set_listen_only(config[CONF_LISTEN_ONLY]))
    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))
