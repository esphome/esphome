import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import DEVICE_CLASS_SWITCH, ENTITY_CATEGORY_CONFIG

from .. import CONF_LD2410S_ID, LD2410S, ld2410s_ns

LD2410SMinimalOutputSwitch = ld2410s_ns.class_(
    "LD2410SMinimalOutputSwitch", switch.Switch
)

CONF_MINIMAL_OUTPUT = "minimal_output"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410S),
    cv.Optional(CONF_MINIMAL_OUTPUT): switch.switch_schema(
        LD2410SMinimalOutputSwitch,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon="mdi:arrow-collapse-horizontal",
    ),
}


async def to_code(config):
    ld2410s_component = await cg.get_variable(config[CONF_LD2410S_ID])
    if minimal_output_config := config.get(CONF_MINIMAL_OUTPUT):
        s = await switch.new_switch(minimal_output_config)
        await cg.register_parented(s, config[CONF_LD2410S_ID])
        cg.add(ld2410s_component.set_minimal_output_switch(s))
