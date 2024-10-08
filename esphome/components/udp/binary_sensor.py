import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.config_validation import All, has_at_least_one_key
from esphome.const import CONF_ID

from . import (
    CONF_PROVIDER,
    CONF_REMOTE_ID,
    CONF_UDP_ID,
    SENSOR_SCHEMA,
    require_internal_with_name,
)

DEPENDENCIES = ["udp"]

CONFIG_SCHEMA = All(
    binary_sensor.binary_sensor_schema().extend(SENSOR_SCHEMA),
    has_at_least_one_key(CONF_ID, CONF_REMOTE_ID),
    require_internal_with_name,
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    comp = await cg.get_variable(config[CONF_UDP_ID])
    remote_id = str(config.get(CONF_REMOTE_ID) or config.get(CONF_ID))
    cg.add(comp.add_remote_binary_sensor(config[CONF_PROVIDER], remote_id, var))
