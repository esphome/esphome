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
    CONF_INITIAL_VALUE,
)
from .. import tuya_ns, CONF_TUYA_ID, Tuya, TuyaDatapointType

DEPENDENCIES = ["tuya"]
CODEOWNERS = ["@frankiboy1"]

CONF_DATAPOINT_HIDDEN = "datapoint_hidden"
CONF_DATAPOINT_TYPE = "datapoint_type"

TuyaNumber = tuya_ns.class_("TuyaNumber", number.Number, cg.Component)

DATAPOINT_TYPES = {
    "int": TuyaDatapointType.INTEGER,
    "uint": TuyaDatapointType.INTEGER,
    "enum": TuyaDatapointType.ENUM,
}


def validate_min_max(config):
    max_value = config[CONF_MAX_VALUE]
    min_value = config[CONF_MIN_VALUE]
    if max_value <= min_value:
        raise cv.Invalid("max_value must be greater than min_value")
    if hidden_config := config.get(CONF_DATAPOINT_HIDDEN):
        if (initial_value := hidden_config.get(CONF_INITIAL_VALUE, None)) is not None:
            if (initial_value > max_value) or (initial_value < min_value):
                raise cv.Invalid(
                    f"{CONF_INITIAL_VALUE} must be a value between {CONF_MAX_VALUE} and {CONF_MIN_VALUE}"
                )
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
            cv.Optional(CONF_DATAPOINT_HIDDEN): cv.All(
                cv.Schema(
                    {
                        cv.Required(CONF_DATAPOINT_TYPE): cv.enum(
                            DATAPOINT_TYPES, lower=True
                        ),
                        cv.Optional(CONF_INITIAL_VALUE): cv.float_,
                    }
                )
            ),
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
    if hidden_config := config.get(CONF_DATAPOINT_HIDDEN):
        cg.add(var.set_datapoint_type(hidden_config[CONF_DATAPOINT_TYPE]))
        if (
            hidden_init_value := hidden_config.get(CONF_INITIAL_VALUE, None)
        ) is not None:
            cg.add(var.set_datapoint_initial_value(hidden_init_value))
