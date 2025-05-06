import esphome.codegen as cg
from esphome.components.sensor import new_sensor, sensor_schema
from esphome.const import CONF_ID

from . import (
    CONF_PROVIDER,
    CONF_REMOTE_ID,
    CONF_TRANSPORT_ID,
    packet_transport_sensor_schema,
)

CONFIG_SCHEMA = packet_transport_sensor_schema(sensor_schema())


async def to_code(config):
    var = await new_sensor(config)
    comp = await cg.get_variable(config[CONF_TRANSPORT_ID])
    remote_id = str(config.get(CONF_REMOTE_ID) or config.get(CONF_ID))
    cg.add(comp.add_remote_sensor(config[CONF_PROVIDER], remote_id, var))
