import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_BLUETOOTH,
)
from .. import CONF_AT581X_ID, AT581XComponent, at581x_ns

RFSwitch = at581x_ns.class_("RFSwitch", switch.Switch)

CONF_RF = "rf"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_AT581X_ID): cv.use_id(AT581XComponent),
    cv.Optional(CONF_RF): switch.switch_schema(
        RFSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_BLUETOOTH,
    ),
}


async def to_code(config):
    at581x_component = await cg.get_variable(config[CONF_AT581X_ID])
    if rf_config := config.get(CONF_RF):
        s = await switch.new_switch(rf_config)
        await cg.register_parented(s, config[CONF_AT581X_ID])
        cg.add(at581x_component.set_rf_switch(s))
