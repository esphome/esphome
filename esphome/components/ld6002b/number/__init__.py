import esphome.codegen as cg
from esphome.components import number
from esphome.components.button import DOMAIN as BUTTON_DOMAIN
from esphome.components.number import DOMAIN as NUMBER_DOMAIN
from esphome.components.select import DOMAIN as SELECT_DOMAIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_AREA_ID,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_CONFIG,
    UNIT_METER,
    UNIT_MILLISECOND,
    UNIT_SECOND,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from .. import LD6002BComponent, validate_unique_settings
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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD6002B_ID): cv.use_id(LD6002BComponent),
        cv.Optional(CONF_HOLD_DELAY): number.number_schema(
            number.ApplyNumber,
            unit_of_measurement=UNIT_SECOND,
            device_class=DEVICE_CLASS_DURATION,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_Z_MIN): number.number_schema(
            number.ApplyNumber,
            unit_of_measurement=UNIT_METER,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_Z_MAX): number.number_schema(
            number.ApplyNumber,
            unit_of_measurement=UNIT_METER,
            device_class=DEVICE_CLASS_DISTANCE,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_LOW_POWER_SLEEP_TIME): number.number_schema(
            number.ApplyNumber,
            unit_of_measurement=UNIT_MILLISECOND,
            device_class=DEVICE_CLASS_DURATION,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_AREA_CONFIG): cv.Schema(
            {
                cv.Optional(KEY_X_MIN): number.number_schema(
                    number.ApplyNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(KEY_X_MAX): number.number_schema(
                    number.ApplyNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(KEY_Y_MIN): number.number_schema(
                    number.ApplyNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(KEY_Y_MAX): number.number_schema(
                    number.ApplyNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_Z_MIN): number.number_schema(
                    number.ApplyNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_Z_MAX): number.number_schema(
                    number.ApplyNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            }
        ),
    }
)


def final_validate(config: ConfigType) -> None:
    validate_unique_settings(NUMBER_DOMAIN, config)
    if config.get(CONF_AREA_CONFIG) is None:
        return

    full_config = fv.full_config.get()
    hub_id = config[CONF_LD6002B_ID]

    has_apply_area = any(
        entry.get(CONF_LD6002B_ID) == hub_id and entry.get(CONF_APPLY_AREA) is not None
        for entry in full_config.get(BUTTON_DOMAIN, [])
    )
    if not has_apply_area:
        raise cv.Invalid(
            f"{CONF_AREA_CONFIG} requires button.apply_area for the same ld6002b instance",
            path=[CONF_AREA_CONFIG],
        )

    has_area_id_select = any(
        entry.get(CONF_LD6002B_ID) == hub_id and entry.get(CONF_AREA_ID) is not None
        for entry in full_config.get(SELECT_DOMAIN, [])
    )
    if not has_area_id_select:
        raise cv.Invalid(
            f"{CONF_AREA_CONFIG} requires select.area_id for the same ld6002b instance",
            path=[CONF_AREA_CONFIG],
        )


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD6002B_ID])
    control = {"apply": "set_number_value(entity, value)", "publish": "before"}
    numbers = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_HOLD_DELAY,
        hub.set_hold_delay_number,
        min_value=0,
        max_value=65535,
        step=1,
        **control,
    )
    meters = {"min_value": -10, "max_value": 10, "step": 0.1, **control}
    await numbers(CONF_Z_MIN, hub.set_z_min_number, **meters)
    await numbers(CONF_Z_MAX, hub.set_z_max_number, **meters)
    # 0x0205 is a uint32 of ms with no documented maximum; cap it at a minute
    await numbers(
        CONF_LOW_POWER_SLEEP_TIME,
        hub.set_low_power_sleep_number,
        min_value=0,
        max_value=60000,
        step=100,
        **control,
    )

    if area_config := config.get(CONF_AREA_CONFIG):
        area = number.sub_numbers(area_config, parent=hub)
        await area(KEY_X_MIN, hub.set_area_x_min_number, **meters)
        await area(KEY_X_MAX, hub.set_area_x_max_number, **meters)
        await area(KEY_Y_MIN, hub.set_area_y_min_number, **meters)
        await area(KEY_Y_MAX, hub.set_area_y_max_number, **meters)
        await area(CONF_Z_MIN, hub.set_area_z_min_number, **meters)
        await area(CONF_Z_MAX, hub.set_area_z_max_number, **meters)
