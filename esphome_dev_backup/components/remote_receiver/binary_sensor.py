from esphome.components import binary_sensor, remote_base

DEPENDENCIES = ["remote_receiver"]

CONFIG_SCHEMA = remote_base.validate_binary_sensor


async def to_code(config):
    var = await remote_base.build_binary_sensor(config)
    await binary_sensor.register_binary_sensor(var, config)
