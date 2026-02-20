import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.const import CONF_ID

from .. import (
    CONF_MK2PVROUTER_ID,
    CONF_TAG_NAME,
    MK2PVROUTER_LISTENER_SCHEMA,
    mk2pvrouter_ns,
)

Mk2PVRouterBinarySensor = mk2pvrouter_ns.class_(
    "Mk2PVRouterBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(Mk2PVRouterBinarySensor).extend(
    MK2PVROUTER_LISTENER_SCHEMA
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await binary_sensor.register_binary_sensor(var, config)
    mk2pvrouter = await cg.get_variable(config[CONF_MK2PVROUTER_ID])
    cg.add(mk2pvrouter.register_mk2pvrouter_listener(var))
