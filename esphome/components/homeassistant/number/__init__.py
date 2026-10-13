import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    final_validate_entity_id,
    homeassistant_ns,
    setup_home_assistant_entity,
    validate_entity_domain,
)

CODEOWNERS = ["@landonr"]
DEPENDENCIES = ["api"]

SUPPORTED_DOMAINS = ["input_number", "number"]

HomeassistantNumber = homeassistant_ns.class_(
    "HomeassistantNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = cv.All(
    number.number_schema(HomeassistantNumber)
    .extend(HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_entity_domain("number", SUPPORTED_DOMAINS),
)

FINAL_VALIDATE_SCHEMA = final_validate_entity_id


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_API_HOMEASSISTANT_SERVICES")
    var = await number.new_number(
        config,
        min_value=0,
        max_value=0,
        step=0,
    )
    await cg.register_component(var, config)
    setup_home_assistant_entity(var, config)
