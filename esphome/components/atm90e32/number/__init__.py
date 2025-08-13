import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_VALUE,
    CONF_MIN_VALUE,
    CONF_MODE,
    CONF_PHASE_A,
    CONF_PHASE_B,
    CONF_PHASE_C,
    CONF_REFERENCE_VOLTAGE,
    CONF_STEP,
    ENTITY_CATEGORY_CONFIG,
    UNIT_AMPERE,
    UNIT_VOLT,
)

from .. import atm90e32_ns
from ..sensor import ATM90E32Component

ATM90E32Number = atm90e32_ns.class_(
    "ATM90E32Number", number.Number, cg.Parented.template(ATM90E32Component)
)

CONF_REFERENCE_CURRENT = "reference_current"
PHASE_KEYS = [CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C]


REFERENCE_VOLTAGE_PHASE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MODE, default="box"): cv.string,
            cv.Optional(CONF_MIN_VALUE, default=100.0): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=260.0): cv.float_,
            cv.Optional(CONF_STEP, default=0.1): cv.float_,
        }
    ).extend(
        number.number_schema(
            class_=ATM90E32Number,
            unit_of_measurement=UNIT_VOLT,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:power-plug",
        )
    )
)


REFERENCE_CURRENT_PHASE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_MODE, default="box"): cv.string,
            cv.Optional(CONF_MIN_VALUE, default=1.0): cv.float_,
            cv.Optional(CONF_MAX_VALUE, default=200.0): cv.float_,
            cv.Optional(CONF_STEP, default=0.1): cv.float_,
        }
    ).extend(
        number.number_schema(
            class_=ATM90E32Number,
            unit_of_measurement=UNIT_AMPERE,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:home-lightning-bolt",
        )
    )
)


REFERENCE_VOLTAGE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PHASE_A): REFERENCE_VOLTAGE_PHASE_SCHEMA,
        cv.Optional(CONF_PHASE_B): REFERENCE_VOLTAGE_PHASE_SCHEMA,
        cv.Optional(CONF_PHASE_C): REFERENCE_VOLTAGE_PHASE_SCHEMA,
    }
)

REFERENCE_CURRENT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PHASE_A): REFERENCE_CURRENT_PHASE_SCHEMA,
        cv.Optional(CONF_PHASE_B): REFERENCE_CURRENT_PHASE_SCHEMA,
        cv.Optional(CONF_PHASE_C): REFERENCE_CURRENT_PHASE_SCHEMA,
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(ATM90E32Component),
        cv.Optional(CONF_REFERENCE_VOLTAGE): REFERENCE_VOLTAGE_SCHEMA,
        cv.Optional(CONF_REFERENCE_CURRENT): REFERENCE_CURRENT_SCHEMA,
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ID])

    if voltage_cfg := config.get(CONF_REFERENCE_VOLTAGE):
        voltage_objs = [None, None, None]

        for i, key in enumerate(PHASE_KEYS):
            if validated := voltage_cfg.get(key):
                obj = await number.new_number(
                    validated,
                    min_value=validated["min_value"],
                    max_value=validated["max_value"],
                    step=validated["step"],
                )
                await cg.register_parented(obj, parent)
                voltage_objs[i] = obj

        # Inherit from A → B/C if only A defined
        if voltage_objs[0] is not None:
            for i in range(3):
                if voltage_objs[i] is None:
                    voltage_objs[i] = voltage_objs[0]

        for i, obj in enumerate(voltage_objs):
            if obj is not None:
                cg.add(parent.set_reference_voltage(i, obj))

    if current_cfg := config.get(CONF_REFERENCE_CURRENT):
        for i, key in enumerate(PHASE_KEYS):
            if validated := current_cfg.get(key):
                obj = await number.new_number(
                    validated,
                    min_value=validated["min_value"],
                    max_value=validated["max_value"],
                    step=validated["step"],
                )
                await cg.register_parented(obj, parent)
                cg.add(parent.set_reference_current(i, obj))
