import esphome.codegen as cg
from esphome.components import binary_sensor
from esphome.components.jablotron import (
    CONF_INDEX,
    INDEX_SCHEMA,
    JABLOTRON_DEVICE_SCHEMA,
    register_jablotron_device,
)

jablotron_peripheral_ns = cg.esphome_ns.namespace("jablotron_peripheral")
PeripheralSensor = jablotron_peripheral_ns.class_(
    "PeripheralSensor", binary_sensor.BinarySensor
)

DEPENDENCIES = ["jablotron"]
CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(PeripheralSensor)
    .extend(JABLOTRON_DEVICE_SCHEMA)
    .extend(INDEX_SCHEMA)
)


async def to_code(config):
    sensor = await binary_sensor.new_binary_sensor(config)
    cg.add(sensor.set_index(config[CONF_INDEX]))
    await register_jablotron_device(sensor, config)
