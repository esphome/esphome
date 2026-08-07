import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_CONFIG,
    UNIT_METER,
    UNIT_MILLISECOND,
    UNIT_SECOND,
)

from .. import LD6002BComponent, ld6002b_ns
from ..const import (
    CONF_HOLD_DELAY,
    CONF_LD6002B_ID,
    CONF_LOW_POWER_SLEEP_TIME,
    CONF_Z_MAX,
    CONF_Z_MIN,
)

DEPENDENCIES = ["ld6002b"]

LD6002BNumber = ld6002b_ns.class_("LD6002BNumber", number.Number)
NumberType = ld6002b_ns.enum("NumberType", is_class=True)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_HOLD_DELAY): number.number_schema(
            LD6002BNumber,
            unit_of_measurement=UNIT_SECOND,
            device_class=DEVICE_CLASS_DURATION,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_Z_MIN): number.number_schema(
            LD6002BNumber,
            unit_of_measurement=UNIT_METER,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_Z_MAX): number.number_schema(
            LD6002BNumber,
            unit_of_measurement=UNIT_METER,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_LOW_POWER_SLEEP_TIME): number.number_schema(
            LD6002BNumber,
            unit_of_measurement=UNIT_MILLISECOND,
            device_class=DEVICE_CLASS_DURATION,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_LD6002B_ID])

    for key, number_type, setter, min_value, max_value, step in (
        (CONF_HOLD_DELAY, NumberType.HOLD_DELAY, "set_hold_delay_number", 0, 65535, 1),
        (CONF_Z_MIN, NumberType.Z_MIN, "set_z_min_number", -10, 10, 0.1),
        (CONF_Z_MAX, NumberType.Z_MAX, "set_z_max_number", -10, 10, 0.1),
        # 0x0205 carries a uint32 of milliseconds; the vendor documents 500 ms as
        # the default and no upper bound, so the range ends at a minute rather
        # than at a default the module is free to be sleeping past.
        (
            CONF_LOW_POWER_SLEEP_TIME,
            NumberType.LOW_POWER_SLEEP,
            "set_low_power_sleep_number",
            0,
            60000,
            100,
        ),
    ):
        if conf := config.get(key):
            n = await number.new_number(
                conf, number_type, min_value=min_value, max_value=max_value, step=step
            )
            await cg.register_parented(n, config[CONF_LD6002B_ID])
            cg.add(getattr(hub, setter)(n))
