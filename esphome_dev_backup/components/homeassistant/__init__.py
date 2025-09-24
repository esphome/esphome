import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ATTRIBUTE, CONF_ENTITY_ID, CONF_INTERNAL

CODEOWNERS = ["@OttoWinter", "@esphome/core"]
homeassistant_ns = cg.esphome_ns.namespace("homeassistant")


def validate_entity_domain(platform, supported_domains):
    def validator(config):
        domain = config[CONF_ENTITY_ID].split(".", 1)[0]
        if domain not in supported_domains:
            raise cv.Invalid(
                f"Entity ID {config[CONF_ENTITY_ID]} is not supported by the {platform} platform."
            )
        return config

    return validator


HOME_ASSISTANT_IMPORT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY_ID): cv.entity_id,
        cv.Optional(CONF_ATTRIBUTE): cv.string,
        cv.Optional(CONF_INTERNAL, default=True): cv.boolean,
    }
)

HOME_ASSISTANT_IMPORT_CONTROL_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY_ID): cv.entity_id,
        cv.Optional(CONF_INTERNAL, default=True): cv.boolean,
    }
)


def setup_home_assistant_entity(var, config):
    cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
    if CONF_ATTRIBUTE in config:
        cg.add(var.set_attribute(config[CONF_ATTRIBUTE]))
