from collections.abc import Callable, Iterable

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ATTRIBUTE, CONF_ENTITY_ID, CONF_ID, CONF_INTERNAL
from esphome.cpp_generator import MockObj
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@OttoWinter", "@esphome/core"]
homeassistant_ns = cg.esphome_ns.namespace("homeassistant")


def validate_entity_domain(
    platform: str, supported_domains: Iterable[str]
) -> Callable[[ConfigType], ConfigType]:
    def validator(config: ConfigType) -> ConfigType:
        # A wizard input supplies the entity ID later; the wizard checks its domains
        if (entity_id := config.get(CONF_ENTITY_ID)) is None:
            return config
        if entity_id.split(".", 1)[0] not in supported_domains:
            raise cv.Invalid(
                f"Entity ID {entity_id} is not supported by the {platform} platform."
            )
        return config

    return validator


HOME_ASSISTANT_IMPORT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENTITY_ID): cv.entity_id,
        cv.Optional(CONF_ATTRIBUTE): cv.string,
        cv.Optional(CONF_INTERNAL, default=True): cv.boolean,
    }
)

HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENTITY_ID): cv.entity_id,
        cv.Optional(CONF_INTERNAL, default=True): cv.boolean,
    }
)


def final_validate_entity_id(config: ConfigType) -> ConfigType:
    """Without an entity_id, the entity must be a wizard input, as Home Assistant then supplies it."""
    if CONF_ENTITY_ID in config:
        return config
    from esphome.components import api

    if config[CONF_ID].id not in api.wizard_input_ids(fv.full_config.get()[api.DOMAIN]):
        raise cv.Invalid(
            f"{CONF_ENTITY_ID} is required unless this entity is a wizard input"
        )
    return config


def setup_home_assistant_entity(var: MockObj, config: ConfigType) -> None:
    from esphome.components import api

    if (buffer := api.wizard_input_buffer(config[CONF_ID])) is not None:
        # The entity ID is chosen in the wizard and lives in a buffer the API owns
        cg.add(var.set_entity_id(cg.RawExpression(buffer)))
    else:
        cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
    if CONF_ATTRIBUTE in config:
        cg.add(var.set_attribute(config[CONF_ATTRIBUTE]))
    cg.add_define("USE_API_HOMEASSISTANT_STATES")
