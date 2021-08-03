import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_CHANNEL, CONF_ID
from . import ttp229_bsf_ns, TTP229BSFComponent, CONF_TTP229_ID

DEPENDENCIES = ["ttp229_bsf"]
TTP229BSFChannel = ttp229_bsf_ns.class_("TTP229BSFChannel", binary_sensor.BinarySensor)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TTP229BSFChannel),
        cv.GenerateID(CONF_TTP229_ID): cv.use_id(TTP229BSFComponent),
        cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)

    cg.add(var.set_channel(config[CONF_CHANNEL]))
    hub = await cg.get_variable(config[CONF_TTP229_ID])
    cg.add(hub.register_channel(var))
