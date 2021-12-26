import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_ID, CONF_KEY


CONF_TM1638_ID = "tm1638_id"

tm1638_ns = cg.esphome_ns.namespace("tm1638")
TM1638Component = tm1638_ns.class_("TM1638Component", cg.PollingComponent)
TM1638Key = tm1638_ns.class_("TM1638Key", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TM1638Key),
        cv.GenerateID(CONF_TM1638_ID): cv.use_id(TM1638Component),
        cv.Required(CONF_KEY): cv.int_range(min=0, max=15),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)
    cg.add(var.set_keycode(config[CONF_KEY]))
    hub = await cg.get_variable(config[CONF_TM1638_ID])
    cg.add(hub.add_tm1638_key(var))
