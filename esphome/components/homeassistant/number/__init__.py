import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number, homeassistant
from esphome.const import CONF_ENTITY_ID, CONF_NAME, CONF_ID, CONF_INTERNAL

CODEOWNERS = ["@landonr"]
homeassistant_number_ns = cg.esphome_ns.namespace("homeassistant_number")

AUTO_LOAD = ["number", "homeassistant"]

HomeAssistantNumber = homeassistant_number_ns.class_(
    "HomeAssistantNumber", number.Number, cg.Component, cg.EntityBase
)

CONFIG_SCHEMA = number.NUMBER_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(HomeAssistantNumber),
        cv.Required(CONF_ENTITY_ID): cv.entity_id,
        cv.Required(CONF_NAME): cv.string,
        cv.Optional(CONF_INTERNAL, default=True): cv.boolean,
    }
).extend(homeassistant.COMPONENT_CONFIG_SCHEMA)


async def to_code(config):
    cg.add_build_flag("-DUSE_API_NUMBER")
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=0,
        max_value=0,
        step=0,
    )
    homeassistant.base_to_code(var, config)
    return var
