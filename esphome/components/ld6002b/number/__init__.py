import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUTTON,
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_CONFIG,
    UNIT_METER,
    UNIT_MILLISECOND,
    UNIT_SECOND,
)
import esphome.final_validate as fv

from .. import LD6002BComponent, ld6002b_ns
from ..const import (
    CONF_AREA_CONFIG,
    CONF_HOLD_DELAY,
    CONF_LD6002B_ID,
    CONF_LOW_POWER_SLEEP_TIME,
    CONF_Z_MAX,
    CONF_Z_MIN,
    KEY_X_MAX,
    KEY_X_MIN,
    KEY_Y_MAX,
    KEY_Y_MIN,
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
        cv.Optional(CONF_AREA_CONFIG): cv.Schema(
            {
                cv.Optional(KEY_X_MIN): number.number_schema(
                    LD6002BNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(KEY_X_MAX): number.number_schema(
                    LD6002BNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(KEY_Y_MIN): number.number_schema(
                    LD6002BNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(KEY_Y_MAX): number.number_schema(
                    LD6002BNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
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
            }
        ),
    }
)


def final_validate(config):
    if config.get(CONF_AREA_CONFIG) is None:
        return config

    full_config = fv.full_config.get()
    hub_id = config[CONF_LD6002B_ID]

    has_apply_area = any(
        entry.get(CONF_LD6002B_ID) == hub_id and entry.get("apply_area") is not None
        for entry in full_config.get(CONF_BUTTON, [])
    )
    if not has_apply_area:
        raise cv.Invalid(
            f"{CONF_AREA_CONFIG} requires button.apply_area for the same ld6002b instance",
            path=[CONF_AREA_CONFIG],
        )

    has_area_id_select = any(
        entry.get(CONF_LD6002B_ID) == hub_id and entry.get("area_id") is not None
        for entry in full_config.get("select", [])
    )
    if not has_area_id_select:
        raise cv.Invalid(
            f"{CONF_AREA_CONFIG} requires select.area_id for the same ld6002b instance",
            path=[CONF_AREA_CONFIG],
        )

    return config


FINAL_VALIDATE_SCHEMA = final_validate


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
            n, low_power_sleep_config, min_value=0, max_value=600000, step=100
        )
        await cg.register_parented(n, config[CONF_LD6002B_ID])
        cg.add(hub.set_low_power_sleep_number(n))

    if area_config := config.get(CONF_AREA_CONFIG):
        if x_min_config := area_config.get(KEY_X_MIN):
            n = cg.new_Pvariable(x_min_config[CONF_ID], NumberType.AREA_X_MIN)
            await number.register_number(
                n, x_min_config, min_value=-10, max_value=10, step=0.1
            )
            await cg.register_parented(n, config[CONF_LD6002B_ID])
            cg.add(hub.set_area_x_min_number(n))
        if x_max_config := area_config.get(KEY_X_MAX):
            n = cg.new_Pvariable(x_max_config[CONF_ID], NumberType.AREA_X_MAX)
            await number.register_number(
                n, x_max_config, min_value=-10, max_value=10, step=0.1
            )
            await cg.register_parented(n, config[CONF_LD6002B_ID])
            cg.add(hub.set_area_x_max_number(n))
        if y_min_config := area_config.get(KEY_Y_MIN):
            n = cg.new_Pvariable(y_min_config[CONF_ID], NumberType.AREA_Y_MIN)
            await number.register_number(
                n, y_min_config, min_value=-10, max_value=10, step=0.1
            )
            await cg.register_parented(n, config[CONF_LD6002B_ID])
            cg.add(hub.set_area_y_min_number(n))
        if y_max_config := area_config.get(KEY_Y_MAX):
            n = cg.new_Pvariable(y_max_config[CONF_ID], NumberType.AREA_Y_MAX)
            await number.register_number(
                n, y_max_config, min_value=-10, max_value=10, step=0.1
            )
            await cg.register_parented(n, config[CONF_LD6002B_ID])
            cg.add(hub.set_area_y_max_number(n))
        if z_min_config := area_config.get(CONF_Z_MIN):
            n = cg.new_Pvariable(z_min_config[CONF_ID], NumberType.AREA_Z_MIN)
            await number.register_number(
                n, z_min_config, min_value=-10, max_value=10, step=0.1
            )
            await cg.register_parented(n, config[CONF_LD6002B_ID])
            cg.add(hub.set_area_z_min_number(n))
        if z_max_config := area_config.get(CONF_Z_MAX):
            n = cg.new_Pvariable(z_max_config[CONF_ID], NumberType.AREA_Z_MAX)
            await number.register_number(
                n, z_max_config, min_value=-10, max_value=10, step=0.1
            )
            await cg.register_parented(n, config[CONF_LD6002B_ID])
            cg.add(hub.set_area_z_max_number(n))
