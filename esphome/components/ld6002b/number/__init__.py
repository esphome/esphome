import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
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

    if hold_delay_config := config.get(CONF_HOLD_DELAY):
        n = cg.new_Pvariable(hold_delay_config[CONF_ID], NumberType.HOLD_DELAY)
        await number.register_number(
            n, hold_delay_config, min_value=0, max_value=65535, step=1
        )
        await cg.register_parented(n, config[CONF_LD6002B_ID])
        cg.add(hub.set_hold_delay_number(n))

    if z_min_config := config.get(CONF_Z_MIN):
        n = cg.new_Pvariable(z_min_config[CONF_ID], NumberType.Z_MIN)
        await number.register_number(
            n, z_min_config, min_value=-10, max_value=10, step=0.1
        )
        await cg.register_parented(n, config[CONF_LD6002B_ID])
        cg.add(hub.set_z_min_number(n))

    if z_max_config := config.get(CONF_Z_MAX):
        n = cg.new_Pvariable(z_max_config[CONF_ID], NumberType.Z_MAX)
        await number.register_number(
            n, z_max_config, min_value=-10, max_value=10, step=0.1
        )
        await cg.register_parented(n, config[CONF_LD6002B_ID])
        cg.add(hub.set_z_max_number(n))

    if low_power_sleep_config := config.get(CONF_LOW_POWER_SLEEP_TIME):
        n = cg.new_Pvariable(
            low_power_sleep_config[CONF_ID], NumberType.LOW_POWER_SLEEP
        )
        await number.register_number(
            n, low_power_sleep_config, min_value=0, max_value=500, step=100
        )
        await cg.register_parented(n, config[CONF_LD6002B_ID])
        cg.add(hub.set_low_power_sleep_number(n))
