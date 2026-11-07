import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import (
    HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA,
    final_validate_entity_id,
    home_assistant_entity_id,
    validate_entity_domain,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["api"]

SUPPORTED_DOMAINS = [
    "button",
    "input_button",
]


def _read_wizard_buffer(config: ConfigType) -> ConfigType:
    """The press function reads a wizard input's buffer itself, so the wizard sets no entity ID on it."""
    from esphome.components.api import wizard

    wizard.register_buffer_reader(config[CONF_ID])
    return config


CONFIG_SCHEMA = cv.All(
    button.button_schema(button.ApplyButton).extend(
        HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA
    ),
    validate_entity_domain("button", SUPPORTED_DOMAINS),
    _read_wizard_buffer,
)

FINAL_VALIDATE_SCHEMA = final_validate_entity_id


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_API_HOMEASSISTANT_SERVICES")
    cg.add_define("USE_API_HOMEASSISTANT_STATES")
    entity_id = home_assistant_entity_id(config)
    await button.sub_buttons(config).create(
        config,
        None,
        apply=f"homeassistant::press_button_entity(entity, {entity_id})",
    )
