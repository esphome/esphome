import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from .. import CONF_LORA, Lora, lora_ns

LoraSwitch = lora_ns.class_("LoraSwitch", switch.Switch, cg.Component)

CONF_LORA_PIN = "pin_to_send"
CONFIG_SCHEMA = (
    switch.switch_schema(LoraSwitch, block_inverted=True)
    .extend(
        {
            cv.Required(CONF_LORA): cv.use_id(Lora),
            cv.Required(CONF_LORA_PIN): cv.int_range(min=1, max=256),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await switch.new_switch(config)
    parent = await cg.get_variable(config[CONF_LORA])
    await cg.register_component(var, config)
    cg.add(var.set_parent(parent))
    cg.add(var.set_pin(config[CONF_LORA_PIN]))
