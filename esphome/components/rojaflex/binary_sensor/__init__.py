import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import ROJAFLEX_DEVICE_SCHEMA, RojaflexDevice, register_rojaflex_device, rojaflex_ns

DEPENDENCIES = ["rojaflex"]

BINARY_SENSOR_TYPES = {
    "last_tx_ok": "LAST_TX_OK",
}

RojaflexBinarySensor = rojaflex_ns.class_(
    "RojaflexBinarySensor", binary_sensor.BinarySensor, cg.PollingComponent, RojaflexDevice
)
RojaflexBinarySensorType = rojaflex_ns.enum("RojaflexBinarySensorType", is_class=True)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(RojaflexBinarySensor)
    .extend(ROJAFLEX_DEVICE_SCHEMA)
    .extend({cv.Required(CONF_TYPE): cv.enum(BINARY_SENSOR_TYPES, lower=True)})
    .extend(cv.polling_component_schema("2s"))
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await register_rojaflex_device(var, config)
    cg.add(var.set_sensor_type(getattr(RojaflexBinarySensorType, BINARY_SENSOR_TYPES[config[CONF_TYPE]])))
