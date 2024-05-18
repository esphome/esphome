import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from . import EbyteLoraComponent, CONF_EBYTE_LORA_COMPONENT_ID, ebyte_lora_ns


DEPENDENCIES = ["ebyte_lora"]
EbyteLoraSwitch = ebyte_lora_ns.class_("EbyteLoraSwitch", switch.Switch, cg.Component)

PIN_TO_SEND = "pin_to_send"
CONFIG_SCHEMA = (
    switch.switch_schema(EbyteLoraSwitch)
    .extend(
        {
            cv.GenerateID(CONF_EBYTE_LORA_COMPONENT_ID): cv.use_id(EbyteLoraComponent),
            cv.Required(PIN_TO_SEND): cv.int_range(min=1, max=4),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    ebyte_lora_component = await cg.get_variable(config[CONF_EBYTE_LORA_COMPONENT_ID])
    var = await switch.new_switch(config)
    await cg.register_parented(var, config[CONF_EBYTE_LORA_COMPONENT_ID])
    await cg.register_component(var, config)
    cg.add(var.set_pin(config[PIN_TO_SEND]))
    cg.add(ebyte_lora_component.set_switch(var))
