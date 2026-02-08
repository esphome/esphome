import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_TYPE

from .. import CONF_KEY_NAME, CONF_PARENT_ID, CaravanDeviceComponent, fendt_caravan_ns

FendtSwitch = fendt_caravan_ns.class_(
    "FendtSwitch",
    switch.Switch,
    cg.Component,
    cg.Parented.template(CaravanDeviceComponent),
)


def _switch_schema(key_name_=cv.UNDEFINED) -> cv.Schema:
    return switch.switch_schema(FendtSwitch).extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(CaravanDeviceComponent),
            cv.Optional(CONF_KEY_NAME, default=key_name_): cv.string,
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        "main_switch": _switch_schema(),
        "all_lights": _switch_schema(),
        "floor_heater": _switch_schema("FLOOR_HEATER_ON"),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await switch.new_switch(config)
    if CONF_KEY_NAME in config:
        cg.add(var.set_key_name(config[CONF_KEY_NAME]))
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_switch")(var))
