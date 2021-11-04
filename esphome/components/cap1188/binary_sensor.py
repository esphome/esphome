import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_CHANNEL, CONF_ID
from . import cap1188_ns, CAP1188Component, CONF_CAP1188_ID

DEPENDENCIES = ["cap1188"]
CAP1188Channel = cap1188_ns.class_("CAP1188Channel", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(CAP1188Channel),
        cv.GenerateID(CONF_CAP1188_ID): cv.use_id(CAP1188Component),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=7),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)
    hub = await cg.get_variable(config[CONF_CAP1188_ID])
    cg.add(var.set_channel(config[CONF_CHANNEL]))

    cg.add(hub.register_channel(var))
