import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    UNIT_METER,
    UNIT_REVOLUTIONS_PER_MINUTE,
)
from esphome.types import ConfigType

from .. import Alpha3, alpha3_ns
from ..const import (
    CONF_ALPHA3_ID,
    CONF_CONSTANT_PRESSURE_SETPOINT,
    CONF_CONSTANT_SPEED_SETPOINT,
    CONF_PROPORTIONAL_PRESSURE_SETPOINT,
)

DEPENDENCIES = ["alpha3"]

Alpha3Number = alpha3_ns.class_("Alpha3Number", number.Number)
Alpha3NumberType = alpha3_ns.enum("Alpha3NumberType", is_class=True)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ALPHA3_ID): cv.use_id(Alpha3),
        cv.Optional(CONF_CONSTANT_SPEED_SETPOINT): number.number_schema(
            Alpha3Number,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_REVOLUTIONS_PER_MINUTE,
        ),
        cv.Optional(CONF_CONSTANT_PRESSURE_SETPOINT): number.number_schema(
            Alpha3Number,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_METER,
        ),
        cv.Optional(CONF_PROPORTIONAL_PRESSURE_SETPOINT): number.number_schema(
            Alpha3Number,
            entity_category=ENTITY_CATEGORY_CONFIG,
            unit_of_measurement=UNIT_METER,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ALPHA3_ID])
    for key, number_type, maximum, step in (
        (
            CONF_CONSTANT_SPEED_SETPOINT,
            Alpha3NumberType.ALPHA3_NUMBER_TYPE_CONSTANT_SPEED,
            10000,
            1,
        ),
        (
            CONF_CONSTANT_PRESSURE_SETPOINT,
            Alpha3NumberType.ALPHA3_NUMBER_TYPE_CONSTANT_PRESSURE,
            20,
            0.01,
        ),
        (
            CONF_PROPORTIONAL_PRESSURE_SETPOINT,
            Alpha3NumberType.ALPHA3_NUMBER_TYPE_PROPORTIONAL_PRESSURE,
            20,
            0.01,
        ),
    ):
        if (conf := config.get(key)) is not None:
            entity = await number.new_number(
                conf, hub, number_type, min_value=0, max_value=maximum, step=step
            )
            cg.add(hub.set_setpoint_number(number_type, entity))
