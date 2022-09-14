import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_KEY
from ..display import tm1638_ns, TM1638Component, CONF_TM1638_ID

TM1638Key = tm1638_ns.class_("TM1638Key", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TM1638Key),
        cv.GenerateID(CONF_TM1638_ID): cv.use_id(TM1638Component),
        cv.Required(CONF_KEY): cv.int_range(min=0, max=15),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    cg.add(var.set_keycode(config[CONF_KEY]))
    hub = await cg.get_variable(config[CONF_TM1638_ID])
    cg.add(hub.register_listener(var))
