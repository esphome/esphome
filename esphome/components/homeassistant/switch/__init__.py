import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    final_validate_entity_id,
    homeassistant_ns,
    setup_home_assistant_entity,
    validate_entity_domain,
)

CODEOWNERS = ["@Links2004"]
DEPENDENCIES = ["api"]

SUPPORTED_DOMAINS = [
    "automation",
    "fan",
    "humidifier",
    "input_boolean",
    "light",
    "remote",
    "siren",
    "switch",
]

HomeassistantSwitch = homeassistant_ns.class_(
    "HomeassistantSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = cv.All(
    switch.switch_schema(HomeassistantSwitch)
    .extend(HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    validate_entity_domain("switch", SUPPORTED_DOMAINS),
)

FINAL_VALIDATE_SCHEMA = final_validate_entity_id


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_API_HOMEASSISTANT_SERVICES")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)
    setup_home_assistant_entity(var, config)
