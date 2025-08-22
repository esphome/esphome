import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import (
    CONF_FILTER,
    CONF_INTERRUPT_PIN,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ICON_CHIP,
)
from esphome.cpp_generator import LambdaExpression

from .. import AMG8833, CONF_AMG8833_ID, amg8833_ns

DEPENDENCIES = ["amg8833"]

SetterSwitch = amg8833_ns.class_("SetterSwitch", switch.Switch)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_AMG8833_ID): cv.use_id(AMG8833),
    cv.Optional(CONF_FILTER): switch.switch_schema(
        SetterSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:waveform",
    ),
    cv.Optional(CONF_INTERRUPT_PIN): switch.switch_schema(
        SetterSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_CHIP,
    ),
}


async def to_code(config):
    amg8833_component = await cg.get_variable(config[CONF_AMG8833_ID])
    if filter_config := config.get(CONF_FILTER):
        s = await switch.new_switch(filter_config)
        cg.add(amg8833_component.set_filter_switch(s))
        cg.add(
            s.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->switch_filter(state);",
                    [(bool, "state")],
                )
            )
        )
    if interrupt_pin_config := config.get(CONF_INTERRUPT_PIN):
        s = await switch.new_switch(interrupt_pin_config)
        cg.add(amg8833_component.set_interrupt_pin_switch(s))
        cg.add(
            s.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->switch_interrupt_pin(state);",
                    [(bool, "state")],
                )
            )
        )
