import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import CONF_COMPONENT_ID, CONF_PAGE_ID, CONF_ID
from . import nextion_ns
from .display import Nextion

DEPENDENCIES = ["display"]

CONF_NEXTION_ID = "nextion_id"

NextionTouchComponent = nextion_ns.class_(
    "NextionTouchComponent", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(NextionTouchComponent),
        cv.GenerateID(CONF_NEXTION_ID): cv.use_id(Nextion),
        cv.Required(CONF_PAGE_ID): cv.uint8_t,
        cv.Required(CONF_COMPONENT_ID): cv.uint8_t,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await binary_sensor.register_binary_sensor(var, config)

    hub = await cg.get_variable(config[CONF_NEXTION_ID])
    cg.add(hub.register_touch_component(var))

    cg.add(var.set_component_id(config[CONF_COMPONENT_ID]))
    cg.add(var.set_page_id(config[CONF_PAGE_ID]))
