from esphome.components import number
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_NUMBER_DATAPOINT,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MULTIPLY,
    CONF_STEP,
)
from .. import tuya_ns, CONF_TUYA_ID, Tuya

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@frankiboy1"]

TuyaNumber = tuya_ns.class_("TuyaNumber", number.Number, cg.Component)


def validate_min_max(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    return config


CONFIG_SCHEMA = cv.All(
    number.number_schema(TuyaNumber)
    .extend(
        {
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(Tuya),
            cv.Required(CONF_NUMBER_DATAPOINT): cv.uint8_t,
            cv.Required(CONF_MAX_VALUE): cv.float_,
            cv.Required(CONF_MIN_VALUE): cv.float_,
            cv.Required(CONF_STEP): cv.positive_float,
            cv.Optional(CONF_MULTIPLY, default=1.0): cv.float_,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_min_max,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=config[CONF_MIN_VALUE],
        max_value=config[CONF_MAX_VALUE],
        step=config[CONF_STEP],
    )

    cg.add(var.set_write_multiply(config[CONF_MULTIPLY]))
    parent = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(parent))

    cg.add(var.set_number_id(config[CONF_NUMBER_DATAPOINT]))
