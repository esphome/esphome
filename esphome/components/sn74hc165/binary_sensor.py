import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_PIN
from . import sn74hc165_ns, SN74HC165Component, CONF_SN74HC165_ID

DEPENDENCIES = ["sn74hc165"]
SN74HC165GPIOBinarySensor = sn74hc165_ns.class_(
    "SN74HC165GPIOBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(SN74HC165GPIOBinarySensor).extend(
    {
        cv.GenerateID(CONF_SN74HC165_ID): cv.use_id(SN74HC165Component),
        cv.Required(CONF_PIN): cv.int_range(min=0, max=31),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(var.set_pin(config[CONF_PIN]))
    hub = await cg.get_variable(config[CONF_SN74HC165_ID])
    cg.add(hub.register_input(var))
