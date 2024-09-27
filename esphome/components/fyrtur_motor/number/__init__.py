import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    UNIT_PERCENT,
    ENTITY_CATEGORY_CONFIG,
)
from .. import CONF_FYRTUR_MOTOR_ID, FyrturMotorComponent, fyrtur_motor_ns

SetpointNumber = fyrtur_motor_ns.class_("SetpointNumber", number.Number)

CONF_UPPER_SETPOINT = "upper_setpoint"
CONF_LOWER_SETPOINT = "lower_setpoint"

ICON_UPPER_SETPOINT = "mdi:arrow-collapse-up"
ICON_LOWER_SETPOINT = "mdi:arrow-collapse-down"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_FYRTUR_MOTOR_ID): cv.use_id(FyrturMotorComponent),
        cv.Required(CONF_UPPER_SETPOINT): number.number_schema(
            SetpointNumber,
            unit_of_measurement=UNIT_PERCENT,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_UPPER_SETPOINT,
        ),
        cv.Required(CONF_LOWER_SETPOINT): number.number_schema(
            SetpointNumber,
            unit_of_measurement=UNIT_PERCENT,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_LOWER_SETPOINT,
        ),
    }
)


async def to_code(config):
    fyrtur_motor_component = await cg.get_variable(config[CONF_FYRTUR_MOTOR_ID])
    if upper_setpoint_config := config.get(CONF_UPPER_SETPOINT):
        n = await number.new_number(
            upper_setpoint_config, min_value=0, max_value=100, step=1
        )
        await cg.register_parented(n, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.set_upper_setpoint_number(n))
    if lower_setpoint_config := config.get(CONF_LOWER_SETPOINT):
        n = await number.new_number(
            lower_setpoint_config, min_value=0, max_value=100, step=1
        )
        await cg.register_parented(n, config[CONF_FYRTUR_MOTOR_ID])
        cg.add(fyrtur_motor_component.set_lower_setpoint_number(n))
