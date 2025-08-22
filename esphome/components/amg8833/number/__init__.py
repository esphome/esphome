import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, UNIT_DEGREES
from esphome.cpp_generator import LambdaExpression

from .. import AMG8833, CONF_AMG8833_ID, amg8833_ns

SetterNumber = amg8833_ns.class_("SetterNumber", number.Number)

CONF_PRESENCE_UPPER = "presence_upper"
CONF_PRESENCE_LOWER = "presence_lower"
CONF_PRESENCE_HYSTERESIS = "presence_hysteresis"
CONF_MOTION_MAXIMUM = "motion_maximum"
CONF_MOTION_MINIMUM = "motion_minimum"
CONF_MOTION_HYSTERESIS = "motion_hysteresis"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_AMG8833_ID): cv.use_id(AMG8833),
        cv.Optional(CONF_PRESENCE_UPPER): number.number_schema(
            SetterNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:thermometer-high",
        ),
        cv.Optional(CONF_PRESENCE_LOWER): number.number_schema(
            SetterNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:thermometer-low",
        ),
        cv.Optional(CONF_PRESENCE_HYSTERESIS): number.number_schema(
            SetterNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:swap-vertical",
        ),
        cv.Optional(CONF_MOTION_MAXIMUM): number.number_schema(
            SetterNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:delta",
        ),
        cv.Optional(CONF_MOTION_MINIMUM): number.number_schema(
            SetterNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:delta",
        ),
        cv.Optional(CONF_MOTION_HYSTERESIS): number.number_schema(
            SetterNumber,
            unit_of_measurement=UNIT_DEGREES,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:swap-vertical",
        ),
    }
)


async def to_code(config):
    amg8833_component = await cg.get_variable(config[CONF_AMG8833_ID])
    if number_config := config.get(CONF_MOTION_HYSTERESIS):
        n = await number.new_number(
            number_config,
            min_value=0,
            max_value=100,
            step=0.25,
        )
        cg.add(amg8833_component.set_motion_hysteresis_number(n))
        cg.add(
            n.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->number_motion_hysteresis(value);",
                    [(float, "value")],
                )
            )
        )
    if number_config := config.get(CONF_MOTION_MAXIMUM):
        n = await number.new_number(
            number_config,
            min_value=0,
            max_value=100,
            step=0.25,
        )
        cg.add(amg8833_component.set_motion_maximum_number(n))
        cg.add(
            n.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->number_motion_maximum(value);",
                    [(float, "value")],
                )
            )
        )
    if number_config := config.get(CONF_MOTION_MINIMUM):
        n = await number.new_number(
            number_config,
            min_value=-100,
            max_value=0,
            step=0.25,
        )
        cg.add(amg8833_component.set_motion_minimum_number(n))
        cg.add(
            n.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->number_motion_minimum(value);",
                    [(float, "value")],
                )
            )
        )
    if number_config := config.get(CONF_PRESENCE_HYSTERESIS):
        n = await number.new_number(
            number_config,
            min_value=0,
            max_value=100,
            step=0.25,
        )
        cg.add(amg8833_component.set_presence_hysteresis_number(n))
        cg.add(
            n.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->number_presence_hysteresis(value);",
                    [(float, "value")],
                )
            )
        )
    if number_config := config.get(CONF_PRESENCE_UPPER):
        n = await number.new_number(
            number_config,
            min_value=0,
            max_value=100,
            step=0.25,
        )
        cg.add(amg8833_component.set_presence_upper_number(n))
        cg.add(
            n.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->number_presence_upper(value);",
                    [(float, "value")],
                )
            )
        )
    if number_config := config.get(CONF_PRESENCE_LOWER):
        n = await number.new_number(
            number_config,
            min_value=0,
            max_value=100,
            step=0.25,
        )
        cg.add(amg8833_component.set_presence_lower_number(n))
        cg.add(
            n.set_setter(
                LambdaExpression(
                    f"{amg8833_component}->number_presence_lower(value);",
                    [(float, "value")],
                )
            )
        )
