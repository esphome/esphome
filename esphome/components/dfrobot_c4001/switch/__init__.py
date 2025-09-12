import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH,ENTITY_CATEGORY_CONFIG

from .. import CONF_C4001_ID, dfrobot_c4001_ns, c4001Component


CONF_MOTION_SWITCH = "motion_switch"

C4001Switch = dfrobot_c4001_ns.class_("C4001Switch", switch.Switch)


CONFIG_SCHEMA = {
    cv.GenerateID(CONF_C4001_ID): cv.use_id(c4001Component),
    
    cv.Optional(CONF_MOTION_SWITCH): switch.switch_schema(
        C4001Switch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:radar",
    ),
}

async def to_code(config):
    switch_component = await cg.get_variable(config[CONF_C4001_ID])
    
    if motion_config := config.get(CONF_MOTION_SWITCH):
        sw = await switch.new_switch(motion_config)
        await cg.register_parented(sw, config[CONF_C4001_ID])
        cg.add(switch_component.set_motion_switch(sw))