import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_BYTES,
    CONF_DIV_RATIO,
    CONF_ENTITY_ID,
    CONF_ID,
    CONF_MODE,
    CONF_TYPE,
)

from .. import (
    CONF_DAY_OF_WEEK,
    CONF_OPTOLINK_ID,
    DAY_OF_WEEK,
    SENSOR_BASE_SCHEMA,
    optolink_ns,
)

DEPENDENCIES = ["optolink"]
CODEOWNERS = ["@j0ta29"]

TextType = optolink_ns.enum("TextType")
TYPE = {"DAY_SCHEDULE": TextType.TEXT_TYPE_DAY_SCHEDULE}

OptolinkText = optolink_ns.class_("OptolinkText", text.Text, cg.PollingComponent)


CONFIG_SCHEMA = cv.All(
    text.TEXT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(OptolinkText),
            cv.Optional(CONF_MODE, default="TEXT"): cv.enum(text.TEXT_MODES),
            cv.Required(CONF_TYPE): cv.enum(TYPE, upper=True),
            cv.Required(CONF_ADDRESS): cv.hex_uint32_t,
            cv.Required(CONF_BYTES): cv.one_of(56, int=True),
            cv.Required(CONF_DAY_OF_WEEK): cv.enum(DAY_OF_WEEK, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(SENSOR_BASE_SCHEMA),
)


async def to_code(config):
    component = await cg.get_variable(config[CONF_OPTOLINK_ID])
    var = cg.new_Pvariable(config[CONF_ID], component)

    await cg.register_component(var, config)
    await text.register_text(var, config)

    cg.add(var.set_type(config[CONF_TYPE]))
    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_div_ratio(config[CONF_DIV_RATIO]))
    if CONF_BYTES in config:
        cg.add(var.set_bytes(config[CONF_BYTES]))
    if CONF_DAY_OF_WEEK in config:
        cg.add(var.set_day_of_week(config[CONF_DAY_OF_WEEK]))
    if CONF_ENTITY_ID in config:
        cg.add(var.set_entity_id(config[CONF_ENTITY_ID]))
