import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    final_validate_entity_id,
    homeassistant_ns,
    setup_home_assistant_entity,
    validate_entity_domain,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["api"]

CONF_MAX_OPTIONS = "max_options"
CONF_OPTIONS_BUFFER_SIZE = "options_buffer_size"

SUPPORTED_DOMAINS = [
    "input_select",
    "select",
]

HomeassistantSelect = homeassistant_ns.class_(
    "HomeassistantSelect", select.Select, cg.Component
)

CONFIG_SCHEMA = cv.All(
    select.select_schema(HomeassistantSelect)
    .extend(HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA)
    .extend(
        {
            cv.Optional(CONF_MAX_OPTIONS, default=16): cv.int_range(min=1, max=255),
            cv.Optional(CONF_OPTIONS_BUFFER_SIZE, default=256): cv.int_range(
                min=2, max=4096
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_entity_domain("select", SUPPORTED_DOMAINS),
)

FINAL_VALIDATE_SCHEMA = final_validate_entity_id


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_API_HOMEASSISTANT_SERVICES")
    # Options come from Home Assistant at runtime, into storage reserved during setup
    var = await select.new_select(
        config,
        config[CONF_MAX_OPTIONS],
        config[CONF_OPTIONS_BUFFER_SIZE],
        options=[],
    )
    await cg.register_component(var, config)
    setup_home_assistant_entity(var, config)
