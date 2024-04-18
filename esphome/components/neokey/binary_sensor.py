import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_KEY
from . import neokey_ns, NeoKeyComponent, CONF_NEOKEY_ID

NeoKeyBinarySensor = neokey_ns.class_("NeoKeyBinarySensor", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(NeoKeyBinarySensor).extend(
    {
        cv.GenerateID(CONF_NEOKEY_ID): cv.use_id(NeoKeyComponent),
        cv.Required(CONF_KEY): cv.int_range(min=0, max=3),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_NEOKEY_ID])
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(var.set_key(config[CONF_KEY]))
    cg.add(hub.register_listener(var))
