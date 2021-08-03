import esphome.components.rc522.binary_sensor as rc522_binary_sensor

DEPENDENCIES = ["rc522"]

CONFIG_SCHEMA = rc522_binary_sensor.CONFIG_SCHEMA


async def to_code(config):
    await rc522_binary_sensor.to_code(config)
