from esphome.components.rc522 import binary_sensor

DEPENDENCIES = ['rc522']

CONFIG_SCHEMA = binary_sensor.CONFIG_SCHEMA


def to_code(config):
    binary_sensor.to_code(config)
