import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_RESTORE_VALUE,
    CONF_STEP,
    ENTITY_CATEGORY_CONFIG,
    UNIT_PERCENT,
)

from .. import CONF_BL0940_ID, bl0940_ns
from ..sensor import BL0940

# Define calibration types
CONF_CURRENT_CALIBRATION = "current_calibration"
CONF_VOLTAGE_CALIBRATION = "voltage_calibration"
CONF_POWER_CALIBRATION = "power_calibration"
CONF_ENERGY_CALIBRATION = "energy_calibration"

BL0940Number = bl0940_ns.class_("BL0940Number")

CalibrationNumber = bl0940_ns.class_(
    "CalibrationNumber", number.Number, cg.PollingComponent
)


def validate_min_max(config):
    if config[CONF_MAX_VALUE] <= config[CONF_MIN_VALUE]:
        raise cv.Invalid("max_value must be greater than min_value")
    return config


CALIBRATION_SCHEMA = cv.All(
    number.number_schema(
        CalibrationNumber,
        entity_category=ENTITY_CATEGORY_CONFIG,
        unit_of_measurement=UNIT_PERCENT,
    )
    .extend(
        {
            cv.Optional(CONF_MODE, default="BOX"): cv.enum(number.NUMBER_MODES),
            cv.Optional(CONF_MAX_VALUE, default=10): cv.All(
                cv.float_, cv.Range(max=50)
            ),
            cv.Optional(CONF_MIN_VALUE, default=-10): cv.All(
                cv.float_, cv.Range(min=-50)
            ),
            cv.Optional(CONF_STEP, default=0.1): cv.positive_float,
            cv.Optional(CONF_RESTORE_VALUE): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_min_max,
)

# Configuration schema for BL0940 numbers
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BL0940Number),
        cv.GenerateID(CONF_BL0940_ID): cv.use_id(BL0940),
        cv.Optional(CONF_CURRENT_CALIBRATION): CALIBRATION_SCHEMA,
        cv.Optional(CONF_VOLTAGE_CALIBRATION): CALIBRATION_SCHEMA,
        cv.Optional(CONF_POWER_CALIBRATION): CALIBRATION_SCHEMA,
        cv.Optional(CONF_ENERGY_CALIBRATION): CALIBRATION_SCHEMA,
    }
)


async def to_code(config):
    # Get the BL0940 component instance
    bl0940 = await cg.get_variable(config[CONF_BL0940_ID])

    # Process all calibration types
    for cal_type, setter_method in [
        (CONF_CURRENT_CALIBRATION, "set_current_calibration_number"),
        (CONF_VOLTAGE_CALIBRATION, "set_voltage_calibration_number"),
        (CONF_POWER_CALIBRATION, "set_power_calibration_number"),
        (CONF_ENERGY_CALIBRATION, "set_energy_calibration_number"),
    ]:
        if conf := config.get(cal_type):
            var = await number.new_number(
                conf,
                min_value=conf.get(CONF_MIN_VALUE),
                max_value=conf.get(CONF_MAX_VALUE),
                step=conf.get(CONF_STEP),
            )
            await cg.register_component(var, conf)

            if restore_value := config.get(CONF_RESTORE_VALUE):
                cg.add(var.set_restore_value(restore_value))
            cg.add(getattr(bl0940, setter_method)(var))
