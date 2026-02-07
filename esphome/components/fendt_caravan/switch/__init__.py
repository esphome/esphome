import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import CONF_PARENT_ID, CaravanDeviceComponent, fendt_caravan_ns

FendtSwitch = fendt_caravan_ns.class_(
    "FendtSwitch",
    switch.Switch,
    cg.Component,
    cg.Parented.template(CaravanDeviceComponent),
)

# CONF_MAIN = "main"
# CONF_ALL_LIGHT = "all_light"
# CONF_FLOOR_HEATER = "floor_heater"
#
# SWITCH_UNITS = {
#    CONF_MAIN,
#    CONF_ALL_LIGHT,
#    CONF_FLOOR_HEATER
# }

SwitchType = fendt_caravan_ns.enum("SwitchType")
SWITCH_TYPE = {
    "main_switch": SwitchType.MAIN,
}


# SWITCH_TYPES = {
#    CONF_MAIN: switch.switch_schema(
#        FendtSwitch, default_restore_mode="ALWAYS_OFF", icon="mdi:switch"
#    ),
#    CONF_ALL_LIGHT: switch.switch_schema(
#        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:lamp"
#    ),
#    CONF_FLOOR_HEATER: switch.switch_schema(
#        FendtSwitch, default_restore_mode="RESTORE_DEFAULT_OFF", icon="mdi:heat-wave"
#    ),
# }

CONFIG_SCHEMA = switch.switch_schema(FendtSwitch).extend(
    {
        cv.Required(CONF_TYPE): cv.enum(SWITCH_TYPE, string=True),
        cv.Required(CONF_PARENT_ID): cv.use_id(CaravanDeviceComponent),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_switch")(var))
