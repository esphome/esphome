import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_GAUGE

from ..climate import EquithermClimate, equitherm_climate_ns

EquithermClimateNumber = equitherm_climate_ns.class_(
    "EquithermClimateNumber", number.Number, cg.Component
)

EquithermClimateNumberType = equitherm_climate_ns.enum("EquithermClimateNumberType")

NUMBER_TYPES = {
    "slope": EquithermClimateNumberType.EQUITHERM_NUMBER_SLOPE,
    "exponent": EquithermClimateNumberType.EQUITHERM_NUMBER_EXPONENT,
    "shift": EquithermClimateNumberType.EQUITHERM_NUMBER_SHIFT,
    "target_diff_factor": EquithermClimateNumberType.EQUITHERM_NUMBER_TARGET_DIFF_FACTOR,
    "kp": EquithermClimateNumberType.EQUITHERM_NUMBER_KP,
    "ki": EquithermClimateNumberType.EQUITHERM_NUMBER_KI,
    "kd": EquithermClimateNumberType.EQUITHERM_NUMBER_KD,
}

CONF_CLIMATE_ID = "climate_id"
CONF_TYPE = "type"
CONF_MIN_VALUE = "min_value"
CONF_MAX_VALUE = "max_value"
CONF_STEP = "step"

CONFIG_SCHEMA = (
    number.number_schema(
        EquithermClimateNumber,
        icon=ICON_GAUGE,
        entity_category=ENTITY_CATEGORY_CONFIG,
    )
    .extend(
        {
            cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(EquithermClimate),
            cv.Required(CONF_TYPE): cv.enum(NUMBER_TYPES, lower=True),
            cv.Optional(CONF_MIN_VALUE): cv.float_,
            cv.Optional(CONF_MAX_VALUE): cv.float_,
            cv.Optional(CONF_STEP): cv.positive_float,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_CLIMATE_ID])
    min_value = config.get(CONF_MIN_VALUE, 0.0)
    max_value = config.get(CONF_MAX_VALUE, 100.0)
    step = config.get(CONF_STEP, 1.0)

    var = await number.new_number(
        config, min_value=min_value, max_value=max_value, step=step
    )
    await cg.register_component(var, config)

    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
