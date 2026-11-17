from esphome.components import switch
import esphome.config_validation as cv
from esphome.types import ConfigType

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    final_validate_entity_id,
    homeassistant_ns,
    register_home_assistant_import,
    setup_home_assistant_entity,
    validate_entity_domain,
)

CODEOWNERS = ["@Links2004"]
DEPENDENCIES = ["api"]
AUTO_LOAD = ["homeassistant"]

SUPPORTED_DOMAINS = [
    "automation",
    "fan",
    "humidifier",
    "input_boolean",
    "light",
    "remote",
    "script",
    "siren",
    "switch",
]

HomeassistantSwitch = homeassistant_ns.class_("HomeassistantSwitch", switch.Switch)

CONFIG_SCHEMA = cv.All(
    switch.switch_schema(HomeassistantSwitch).extend(
        HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA
    ),
    validate_entity_domain("switch", SUPPORTED_DOMAINS),
)

FINAL_VALIDATE_SCHEMA = final_validate_entity_id


async def to_code(config: ConfigType) -> None:
    var = await switch.new_switch(config)
    setup_home_assistant_entity(var, config, services=True)
    await register_home_assistant_import(var)
