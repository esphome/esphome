import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    homeassistant_ns,
    setup_home_assistant_entity,
    validate_entity_domain,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["api"]

SUPPORTED_DOMAINS = [
    "button",
    "input_button",
]

HomeassistantButton = homeassistant_ns.class_(
    "HomeassistantButton", button.Button, cg.Component
)

CONFIG_SCHEMA = cv.All(
    button.button_schema(HomeassistantButton)
    .extend(HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_entity_domain("button", SUPPORTED_DOMAINS),
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_API_HOMEASSISTANT_SERVICES")
    var = await button.new_button(config)
    await cg.register_component(var, config)
    setup_home_assistant_entity(var, config)
