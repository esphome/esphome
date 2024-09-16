import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import ENTITY_CATEGORY_CONFIG, CONF_TYPE

from .. import CONF_DFROBOT_SEN0395_ID, DfrobotSen0395Component


DEPENDENCIES = ["dfrobot_sen0395"]

dfrobot_sen0395_ns = cg.esphome_ns.namespace("dfrobot_sen0395")
DfrobotSen0395Switch = dfrobot_sen0395_ns.class_(
    "DfrobotSen0395Switch",
    switch.Switch,
    cg.Component,
    cg.Parented.template(DfrobotSen0395Component),
)

Sen0395PowerSwitch = dfrobot_sen0395_ns.class_(
    "Sen0395PowerSwitch", DfrobotSen0395Switch
)
Sen0395LedSwitch = dfrobot_sen0395_ns.class_("Sen0395LedSwitch", DfrobotSen0395Switch)
Sen0395UartPresenceSwitch = dfrobot_sen0395_ns.class_(
    "Sen0395UartPresenceSwitch", DfrobotSen0395Switch
)
Sen0395StartAfterBootSwitch = dfrobot_sen0395_ns.class_(
    "Sen0395StartAfterBootSwitch", DfrobotSen0395Switch
)

_SWITCH_SCHEMA = (
    switch.switch_schema(
        entity_category=ENTITY_CATEGORY_CONFIG,
    )
    .extend(
        {
            cv.GenerateID(CONF_DFROBOT_SEN0395_ID): cv.use_id(DfrobotSen0395Component),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        "sensor_active": _SWITCH_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(Sen0395PowerSwitch)}
        ),
        "turn_on_led": _SWITCH_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(Sen0395LedSwitch)}
        ),
        "presence_via_uart": _SWITCH_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(Sen0395UartPresenceSwitch)}
        ),
        "start_after_boot": _SWITCH_SCHEMA.extend(
            {cv.GenerateID(): cv.declare_id(Sen0395StartAfterBootSwitch)}
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_DFROBOT_SEN0395_ID])
    var = await switch.new_switch(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_switch")(var))
