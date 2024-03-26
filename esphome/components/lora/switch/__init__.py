import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from .. import CONF_LORA, LoraComponent, lora_ns

LoraSwitch = lora_ns.class_("LoraSwitch", switch.Switch, cg.Component)

PIN_TO_SEND = "pin_to_send"
CONFIG_SCHEMA = (
    switch.switch_schema(LoraSwitch)
    .extend(
        {
            cv.Required(CONF_LORA): cv.use_id(LoraComponent),
            cv.Required(PIN_TO_SEND): cv.int_range(min=1, max=256),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    parent = await cg.get_variable(config[CONF_LORA])
    await cg.register_component(var, config)
    cg.add(var.set_parent(parent))
    cg.add(var.set_pin(config[PIN_TO_SEND]))
