import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ATTRIBUTE,
    CONF_ENTITY_ID,
    CONF_INTERNAL,
    CONF_NAME,
    CONF_DISABLED_BY_DEFAULT,
)

CODEOWNERS = ["@OttoWinter"]
homeassistant_ns = cg.esphome_ns.namespace("homeassistant")

HOME_ASSISTANT_IMPORT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY_ID): cv.entity_id,
        cv.Optional(CONF_ATTRIBUTE): cv.string,
        cv.Optional(CONF_INTERNAL, default=True): cv.boolean,
    }
)

COMPONENT_CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ENTITY_ID): cv.entity_id,
    }
).extend(cv.COMPONENT_SCHEMA)


def setup_home_assistant_entity(var, config):
    cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
    if CONF_ATTRIBUTE in config:
        cg.add(var.set_attribute(config[CONF_ATTRIBUTE]))


def base_to_code(base, config):
    cg.add(base.set_entity_id(config[CONF_ENTITY_ID]))
    if CONF_NAME in config:
        cg.add(base.set_name(config[CONF_NAME]))
    if CONF_DISABLED_BY_DEFAULT in config:
        cg.add(base.set_disabled_by_default(config[CONF_DISABLED_BY_DEFAULT]))

    if CONF_INTERNAL in config:
        cg.add(base.set_internal(config[CONF_INTERNAL]))
    else:
        cg.add(base.set_internal(True))
    return base
