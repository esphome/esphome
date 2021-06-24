import esphome.codegen as cg
from esphome.components import binary_sensor, remote_base
from esphome.const import CONF_NAME, CONF_OBJECT_ID

DEPENDENCIES = ["remote_receiver"]

CONFIG_SCHEMA = remote_base.validate_binary_sensor


async def to_code(config):
    var = await remote_base.build_binary_sensor(config)
    cg.add(var.set_name(config[CONF_NAME]))
    if CONF_OBJECT_ID in config:
        cg.add(var.set_object_id(config[CONF_OBJECT_ID]))
    await binary_sensor.register_binary_sensor(var, config)
