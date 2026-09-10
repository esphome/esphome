import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_AREA_ID,
    CONF_BUTTON,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_CONFIG,
    UNIT_METER,
    UNIT_MILLISECOND,
    UNIT_SECOND,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import LD6002BComponent, ld6002b_ns
from ..const import (
    CONF_APPLY_AREA,
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


def final_validate(config: ConfigType) -> None:
    if config.get(CONF_AREA_CONFIG) is None:
        return

    full_config = fv.full_config.get()
    hub_id = config[CONF_LD6002B_ID]

    has_apply_area = any(
        entry.get(CONF_LD6002B_ID) == hub_id and entry.get(CONF_APPLY_AREA) is not None
        for entry in full_config.get(CONF_BUTTON, [])
    )
    if not has_apply_area:
        raise cv.Invalid(
            f"{CONF_AREA_CONFIG} requires button.apply_area for the same ld6002b instance",
            path=[CONF_AREA_CONFIG],
        )

    has_area_id_select = any(
        entry.get(CONF_LD6002B_ID) == hub_id and entry.get(CONF_AREA_ID) is not None
        for entry in full_config.get("select", [])
    )
    if not has_area_id_select:
        raise cv.Invalid(
            f"{CONF_AREA_CONFIG} requires select.area_id for the same ld6002b instance",
            path=[CONF_AREA_CONFIG],
        )


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
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

    if area_config := config.get(CONF_AREA_CONFIG):
        for key, number_type, setter in (
            (KEY_X_MIN, NumberType.AREA_X_MIN, "set_area_x_min_number"),
            (KEY_X_MAX, NumberType.AREA_X_MAX, "set_area_x_max_number"),
            (KEY_Y_MIN, NumberType.AREA_Y_MIN, "set_area_y_min_number"),
            (KEY_Y_MAX, NumberType.AREA_Y_MAX, "set_area_y_max_number"),
            (CONF_Z_MIN, NumberType.AREA_Z_MIN, "set_area_z_min_number"),
            (CONF_Z_MAX, NumberType.AREA_Z_MAX, "set_area_z_max_number"),
        ):
            if conf := area_config.get(key):
                n = await number.new_number(
                    conf, number_type, min_value=-10, max_value=10, step=0.1
                )
                await cg.register_parented(n, config[CONF_LD6002B_ID])
                cg.add(getattr(hub, setter)(n))
