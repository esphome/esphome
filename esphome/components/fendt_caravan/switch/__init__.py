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

FENDT_SWITCH_SCHEMA = switch.switch_schema(FendtSwitch).extend(
    {
        cv.Required(CONF_PARENT_ID): cv.use_id(CaravanDeviceComponent),
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        "main_switch": FENDT_SWITCH_SCHEMA,
        "all_lights": FENDT_SWITCH_SCHEMA,
        "floor_heater": FENDT_SWITCH_SCHEMA,
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_switch")(var))
