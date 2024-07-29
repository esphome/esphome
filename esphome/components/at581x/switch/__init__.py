import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_SWITCH,
    ICON_WIFI,
)
from .. import CONF_AT581X_ID, AT581XComponent, at581x_ns

DEPENDENCIES = ["at581x"]

RFSwitch = at581x_ns.class_("RFSwitch", switch.Switch)

CONFIG_SCHEMA = switch.switch_schema(
    RFSwitch,
    device_class=DEVICE_CLASS_SWITCH,
    icon=ICON_WIFI,
).extend(
    cv.Schema(
        {
            cv.GenerateID(CONF_AT581X_ID): cv.use_id(AT581XComponent),
        }
    )
)


async def to_code(config):
    at581x_component = await cg.get_variable(config[CONF_AT581X_ID])
    s = await switch.new_switch(config)
    await cg.register_parented(s, config[CONF_AT581X_ID])
    cg.add(at581x_component.set_rf_power_switch(s))
