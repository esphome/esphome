import esphome.codegen as cg
from esphome.components import light
import esphome.config_validation as cv
from esphome.const import (
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_GAMMA_CORRECT,
    CONF_INTERNAL,
    CONF_OUTPUT_ID,
    CONF_RESTORE_MODE,
)

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
)

CODEOWNERS = ["@egormanga"]
DEPENDENCIES = ["api"]

HomeassistantLight = homeassistant_ns.class_(
    "HomeassistantLight", light.LightOutput, cg.Component
)

HOME_ASSISTANT_LIGHT_SCHEMA = {
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HomeassistantLight),
}

CONFIG_SCHEMA = (
    light.LIGHT_SCHEMA.extend(HOME_ASSISTANT_LIGHT_SCHEMA)
    .extend(HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    cg.add_define("USE_API_HOMEASSISTANT_SERVICES")

    config[CONF_DEFAULT_TRANSITION_LENGTH] = 0
    config[CONF_GAMMA_CORRECT] = 0
    config[CONF_INTERNAL] = True
    config[CONF_RESTORE_MODE] = light.RESTORE_MODES["DISABLED"]

    var = await light.new_light(config)
    await cg.register_component(var, config)
    setup_home_assistant_entity(var, config)
