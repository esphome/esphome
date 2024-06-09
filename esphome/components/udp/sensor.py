import esphome.codegen as cg
from esphome.const import CONF_ID
from esphome.config_validation import has_at_least_one_key, All
from esphome.components.sensor import sensor_schema, new_sensor
from . import (
    SENSOR_SCHEMA,
    CONF_UDP_ID,
    CONF_REMOTE_ID,
    CONF_PROVIDER,
    require_internal_with_name,
)

AUTO_LOAD = ["udp"]

CONFIG_SCHEMA = All(
    sensor_schema().extend(SENSOR_SCHEMA),
    has_at_least_one_key(CONF_ID, CONF_REMOTE_ID),
    require_internal_with_name,
)


async def to_code(config):
    var = await new_sensor(config)
    comp = await cg.get_variable(config[CONF_UDP_ID])
    remote_id = str(config.get(CONF_REMOTE_ID) or config.get(CONF_ID))
    cg.add(comp.add_remote_sensor(config[CONF_PROVIDER], remote_id, var))
