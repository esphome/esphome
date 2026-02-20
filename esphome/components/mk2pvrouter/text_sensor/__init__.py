import esphome.codegen as cg
from esphome.components import text_sensor
from esphome.const import CONF_ID

from .. import (
    CONF_MK2PVROUTER_ID,
    CONF_TAG_NAME,
    MK2PVROUTER_LISTENER_SCHEMA,
    mk2pvrouter_ns,
)

Mk2PVRouterTextSensor = mk2pvrouter_ns.class_(
    "Mk2PVRouterTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema(Mk2PVRouterTextSensor).extend(
    MK2PVROUTER_LISTENER_SCHEMA
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_TAG_NAME])
    await cg.register_component(var, config)
    await text_sensor.register_text_sensor(var, config)
    mk2pvrouter = await cg.get_variable(config[CONF_MK2PVROUTER_ID])
    cg.add(mk2pvrouter.register_mk2pvrouter_listener(var))
