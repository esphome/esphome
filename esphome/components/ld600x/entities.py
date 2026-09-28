from collections.abc import Callable
from typing import Any

import esphome.codegen as cg
from esphome.components import (
    binary_sensor,
    button,
    number,
    select,
    sensor,
    switch,
    text_sensor,
)
from esphome.components.const import CONF_TARGET_COUNT
import esphome.config_validation as cv
from esphome.const import (
    CONF_AREA_ID,
    CONF_BUTTON,
    CONF_ID,
    CONF_SENSITIVITY,
    CONF_TARGET,
    CONF_WAKEUP_PIN,
    CONF_X,
    CONF_Y,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_OCCUPANCY,
    DEVICE_CLASS_SWITCH,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
    UNIT_MILLISECOND,
    UNIT_SECOND,
)
from esphome.core import ID
from esphome.core.entity_helpers import SubEntities
from esphome.cpp_generator import MockObj, MockObjClass
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import ld600x_ns
from .const import (
    AREA_COUNT,
    CONF_APPLY_AREA,
    CONF_AREA_CONFIG,
    CONF_AUTO_INTERFERENCE,
    CONF_CLEAR_INTERFERENCE,
    CONF_CLUSTER_ID,
    CONF_DOPPLER_INDEX,
    CONF_GET_AREAS,
    CONF_GET_DELAY,
    CONF_GET_INSTALLATION,
    CONF_GET_LOW_POWER_MODE,
    CONF_GET_LOW_POWER_SLEEP_TIME,
    CONF_GET_SENSITIVITY,
    CONF_GET_TRIGGER_SPEED,
    CONF_GET_Z_RANGE,
    CONF_HOLD_DELAY,
    CONF_INSTALLATION_MODE,
    CONF_LOW_POWER,
    CONF_LOW_POWER_SLEEP_TIME,
    CONF_OTA_VERSION,
    CONF_POINT_CLOUD,
    CONF_POINT_COUNT,
    CONF_RESET_DETECTION_AREA,
    CONF_RESET_UNATTENDED,
    CONF_TARGET_DISPLAY,
    CONF_TRIGGER_SPEED,
    CONF_WAKE,
    CONF_WORK_MODE,
    CONF_Z,
    CONF_Z_MAX,
    CONF_Z_MIN,
    KEY_X_MAX,
    KEY_X_MIN,
    KEY_Y_MAX,
    KEY_Y_MIN,
)

AreaKind = ld600x_ns.enum("AreaKind")


_AREA_KINDS = (
    AreaKind.AREA_KIND_INTERFERENCE,
    AreaKind.AREA_KIND_DETECTION,
    AreaKind.AREA_KIND_DWELL,
)

AreaAxis = ld600x_ns.enum("AreaAxis")


_VALUE_SENSOR_FILTERS = [
    {
        "timeout": {
            "timeout": cv.TimePeriod(milliseconds=1000),
            "value": "last",
        }
    },
    {"throttle_with_priority": cv.TimePeriod(milliseconds=1000)},
]


TARGET_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_X): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            filters=_VALUE_SENSOR_FILTERS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Y): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            filters=_VALUE_SENSOR_FILTERS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Z): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            filters=_VALUE_SENSOR_FILTERS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_DOPPLER_INDEX): sensor.sensor_schema(
            accuracy_decimals=0,
            filters=_VALUE_SENSOR_FILTERS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CLUSTER_ID): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
    }
)


AREA_SCHEMA = cv.Schema(
    {
        cv.Optional(KEY_X_MIN): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(KEY_X_MAX): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(KEY_Y_MIN): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(KEY_Y_MAX): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Z_MIN): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_Z_MAX): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=2,
            device_class=DEVICE_CLASS_DISTANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


_AREA_AXES = (
    (KEY_X_MIN, AreaAxis.AREA_AXIS_X_MIN),
    (KEY_X_MAX, AreaAxis.AREA_AXIS_X_MAX),
    (KEY_Y_MIN, AreaAxis.AREA_AXIS_Y_MIN),
    (KEY_Y_MAX, AreaAxis.AREA_AXIS_Y_MAX),
    (CONF_Z_MIN, AreaAxis.AREA_AXIS_Z_MIN),
    (CONF_Z_MAX, AreaAxis.AREA_AXIS_Z_MAX),
)


def sensor_schema(
    hub_class: MockObjClass,
    hub_id_key: str,
    *,
    max_targets: int,
    area_kinds: tuple[str, ...],
    extra: dict[Any, Any] | None = None,
) -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.GenerateID(hub_id_key): cv.use_id(hub_class),
                cv.Optional(CONF_TARGET_COUNT): sensor.sensor_schema(
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
                cv.Optional(CONF_POINT_COUNT): sensor.sensor_schema(
                    accuracy_decimals=0,
                    state_class=STATE_CLASS_MEASUREMENT,
                ),
            }
        )
        .extend(
            {cv.Optional(f"target_{i + 1}"): TARGET_SCHEMA for i in range(max_targets)}
        )
        .extend(
            {
                cv.Optional(f"{kind}_area_{i}"): AREA_SCHEMA
                for kind in area_kinds
                for i in range(AREA_COUNT)
            }
        )
    ).extend(extra or {})


async def sensor_to_code(
    config: ConfigType,
    hub_id_key: str,
    *,
    max_targets: int,
    area_kinds: tuple[str, ...],
) -> None:
    hub: MockObj = await cg.get_variable(config[hub_id_key])

    sensors: SubEntities = sensor.sub_sensors(config)
    await sensors(CONF_TARGET_COUNT, hub.set_target_count_sensor)
    await sensors(CONF_POINT_COUNT, hub.set_point_count_sensor)

    for i in range(max_targets):
        if target_config := config.get(f"target_{i + 1}"):
            if x_config := target_config.get(CONF_X):
                sens: MockObj = await sensor.new_sensor(x_config)
                cg.add(hub.set_target_x_sensor(i, sens))
            if y_config := target_config.get(CONF_Y):
                sens: MockObj = await sensor.new_sensor(y_config)
                cg.add(hub.set_target_y_sensor(i, sens))
            if z_config := target_config.get(CONF_Z):
                sens: MockObj = await sensor.new_sensor(z_config)
                cg.add(hub.set_target_z_sensor(i, sens))
            if doppler_index_config := target_config.get(CONF_DOPPLER_INDEX):
                sens: MockObj = await sensor.new_sensor(doppler_index_config)
                cg.add(hub.set_target_dop_idx_sensor(i, sens))
            if cluster_id_config := target_config.get(CONF_CLUSTER_ID):
                sens: MockObj = await sensor.new_sensor(cluster_id_config)
                cg.add(hub.set_target_cluster_id_sensor(i, sens))

    for kind_index, kind in enumerate(area_kinds):
        area_kind: MockObj = _AREA_KINDS[kind_index]
        for i in range(AREA_COUNT):
            if area_config := config.get(f"{kind}_area_{i}"):
                for key, axis in _AREA_AXES:
                    if axis_config := area_config.get(key):
                        sens: MockObj = await sensor.new_sensor(axis_config)
                        cg.add(hub.set_area_sensor(area_kind, i, axis, sens))


def binary_sensor_schema(
    hub_class: MockObjClass,
    hub_id_key: str,
    *,
    max_targets: int,
    area_kinds: tuple[str, ...],
    extra: dict[Any, Any] | None = None,
) -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.GenerateID(hub_id_key): cv.use_id(hub_class),
                cv.Optional(CONF_TARGET): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_OCCUPANCY,
                ),
            }
        )
        .extend(
            {
                cv.Optional(f"target_{i + 1}"): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_OCCUPANCY,
                )
                for i in range(max_targets)
            }
        )
        .extend(
            {
                cv.Optional(f"detection_area_{i}"): binary_sensor.binary_sensor_schema(
                    device_class=DEVICE_CLASS_OCCUPANCY,
                )
                for i in range(AREA_COUNT)
            }
        )
    ).extend(extra or {})


async def binary_sensor_to_code(
    config: ConfigType,
    hub_id_key: str,
    *,
    max_targets: int,
    area_kinds: tuple[str, ...],
) -> None:
    hub: MockObj = await cg.get_variable(config[hub_id_key])

    sensors: SubEntities = binary_sensor.sub_binary_sensors(config)
    await sensors(CONF_TARGET, hub.set_presence_binary_sensor)

    for i in range(max_targets):
        if target_config := config.get(f"target_{i + 1}"):
            sens: MockObj = await binary_sensor.new_binary_sensor(target_config)
            cg.add(hub.set_target_presence_binary_sensor(i, sens))

    for i in range(AREA_COUNT):
        if area_config := config.get(f"detection_area_{i}"):
            sens: MockObj = await binary_sensor.new_binary_sensor(area_config)
            cg.add(hub.set_area_presence_binary_sensor(i, sens))


def text_sensor_schema(
    hub_class: MockObjClass, hub_id_key: str, *, extra: dict[Any, Any] | None = None
) -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.GenerateID(hub_id_key): cv.use_id(hub_class),
                cv.Optional(CONF_WORK_MODE): text_sensor.text_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC
                ),
                cv.Optional(CONF_OTA_VERSION): text_sensor.text_sensor_schema(
                    entity_category=ENTITY_CATEGORY_DIAGNOSTIC
                ),
            }
        )
    ).extend(extra or {})


async def text_sensor_to_code(config: ConfigType, hub_id_key: str) -> None:
    hub: MockObj = await cg.get_variable(config[hub_id_key])
    sensors: SubEntities = text_sensor.sub_text_sensors(config)
    await sensors(CONF_WORK_MODE, hub.set_work_mode_text_sensor)
    await sensors(CONF_OTA_VERSION, hub.set_ota_version_text_sensor)


LD600XNumber = ld600x_ns.class_("LD600XNumber", number.Number)


NumberType = ld600x_ns.enum("NumberType")


def number_schema(
    hub_class: MockObjClass, hub_id_key: str, *, extra: dict[Any, Any] | None = None
) -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.GenerateID(hub_id_key): cv.use_id(hub_class),
                cv.Optional(CONF_HOLD_DELAY): number.number_schema(
                    LD600XNumber,
                    unit_of_measurement=UNIT_SECOND,
                    device_class=DEVICE_CLASS_DURATION,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_Z_MIN): number.number_schema(
                    LD600XNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_Z_MAX): number.number_schema(
                    LD600XNumber,
                    unit_of_measurement=UNIT_METER,
                    device_class=DEVICE_CLASS_DISTANCE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_LOW_POWER_SLEEP_TIME): number.number_schema(
                    LD600XNumber,
                    unit_of_measurement=UNIT_MILLISECOND,
                    device_class=DEVICE_CLASS_DURATION,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_AREA_CONFIG): cv.Schema(
                    {
                        cv.Optional(KEY_X_MIN): number.number_schema(
                            LD600XNumber,
                            unit_of_measurement=UNIT_METER,
                            device_class=DEVICE_CLASS_DISTANCE,
                            entity_category=ENTITY_CATEGORY_CONFIG,
                        ),
                        cv.Optional(KEY_X_MAX): number.number_schema(
                            LD600XNumber,
                            unit_of_measurement=UNIT_METER,
                            device_class=DEVICE_CLASS_DISTANCE,
                            entity_category=ENTITY_CATEGORY_CONFIG,
                        ),
                        cv.Optional(KEY_Y_MIN): number.number_schema(
                            LD600XNumber,
                            unit_of_measurement=UNIT_METER,
                            device_class=DEVICE_CLASS_DISTANCE,
                            entity_category=ENTITY_CATEGORY_CONFIG,
                        ),
                        cv.Optional(KEY_Y_MAX): number.number_schema(
                            LD600XNumber,
                            unit_of_measurement=UNIT_METER,
                            device_class=DEVICE_CLASS_DISTANCE,
                            entity_category=ENTITY_CATEGORY_CONFIG,
                        ),
                        cv.Optional(CONF_Z_MIN): number.number_schema(
                            LD600XNumber,
                            unit_of_measurement=UNIT_METER,
                            device_class=DEVICE_CLASS_DISTANCE,
                            entity_category=ENTITY_CATEGORY_CONFIG,
                        ),
                        cv.Optional(CONF_Z_MAX): number.number_schema(
                            LD600XNumber,
                            unit_of_measurement=UNIT_METER,
                            device_class=DEVICE_CLASS_DISTANCE,
                            entity_category=ENTITY_CATEGORY_CONFIG,
                        ),
                    }
                ),
            }
        )
    ).extend(extra or {})


def number_final_validate(
    hub_id_key: str, component_name: str
) -> Callable[[ConfigType], None]:
    def final_validate(config: ConfigType) -> None:
        if config.get(CONF_AREA_CONFIG) is None:
            return

        full_config: fv.FinalValidateConfig = fv.full_config.get()
        hub_id: ID = config[hub_id_key]

        has_apply_area: bool = any(
            entry.get(hub_id_key) == hub_id and entry.get(CONF_APPLY_AREA) is not None
            for entry in full_config.get(CONF_BUTTON, [])
        )
        if not has_apply_area:
            raise cv.Invalid(
                f"{CONF_AREA_CONFIG} requires button.apply_area for the same {component_name} instance",
                path=[CONF_AREA_CONFIG],
            )

        has_area_id_select: bool = any(
            entry.get(hub_id_key) == hub_id and entry.get(CONF_AREA_ID) is not None
            for entry in full_config.get("select", [])
        )
        if not has_area_id_select:
            raise cv.Invalid(
                f"{CONF_AREA_CONFIG} requires select.area_id for the same {component_name} instance",
                path=[CONF_AREA_CONFIG],
            )

    return final_validate


async def number_to_code(config: ConfigType, hub_id_key: str) -> None:
    hub: MockObj = await cg.get_variable(config[hub_id_key])
    numbers: SubEntities = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_HOLD_DELAY,
        hub.set_hold_delay_number,
        NumberType.NUMBER_HOLD_DELAY,
        min_value=0,
        max_value=65535,
        step=1,
    )
    await numbers(
        CONF_Z_MIN,
        hub.set_z_min_number,
        NumberType.NUMBER_Z_MIN,
        min_value=-10,
        max_value=10,
        step=0.1,
    )
    await numbers(
        CONF_Z_MAX,
        hub.set_z_max_number,
        NumberType.NUMBER_Z_MAX,
        min_value=-10,
        max_value=10,
        step=0.1,
    )
    await numbers(
        CONF_LOW_POWER_SLEEP_TIME,
        hub.set_low_power_sleep_number,
        NumberType.NUMBER_LOW_POWER_SLEEP,
        min_value=0,
        max_value=60000,
        step=100,
    )

    if area_config := config.get(CONF_AREA_CONFIG):
        numbers = number.sub_numbers(area_config, parent=hub)
        await numbers(
            KEY_X_MIN,
            hub.set_area_x_min_number,
            NumberType.NUMBER_AREA_X_MIN,
            min_value=-10,
            max_value=10,
            step=0.1,
        )
        await numbers(
            KEY_X_MAX,
            hub.set_area_x_max_number,
            NumberType.NUMBER_AREA_X_MAX,
            min_value=-10,
            max_value=10,
            step=0.1,
        )
        await numbers(
            KEY_Y_MIN,
            hub.set_area_y_min_number,
            NumberType.NUMBER_AREA_Y_MIN,
            min_value=-10,
            max_value=10,
            step=0.1,
        )
        await numbers(
            KEY_Y_MAX,
            hub.set_area_y_max_number,
            NumberType.NUMBER_AREA_Y_MAX,
            min_value=-10,
            max_value=10,
            step=0.1,
        )
        await numbers(
            CONF_Z_MIN,
            hub.set_area_z_min_number,
            NumberType.NUMBER_AREA_Z_MIN,
            min_value=-10,
            max_value=10,
            step=0.1,
        )
        await numbers(
            CONF_Z_MAX,
            hub.set_area_z_max_number,
            NumberType.NUMBER_AREA_Z_MAX,
            min_value=-10,
            max_value=10,
            step=0.1,
        )


LD600XSelect = ld600x_ns.class_("LD600XSelect", select.Select)


SelectType = ld600x_ns.enum("SelectType")


def select_schema(
    hub_class: MockObjClass,
    hub_id_key: str,
    *,
    area_kinds: tuple[str, ...],
    extra: dict[Any, Any] | None = None,
) -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.GenerateID(hub_id_key): cv.use_id(hub_class),
                cv.Optional(CONF_SENSITIVITY): select.select_schema(
                    LD600XSelect, entity_category=ENTITY_CATEGORY_CONFIG
                ),
                cv.Optional(CONF_TRIGGER_SPEED): select.select_schema(
                    LD600XSelect, entity_category=ENTITY_CATEGORY_CONFIG
                ),
                cv.Optional(CONF_INSTALLATION_MODE): select.select_schema(
                    LD600XSelect, entity_category=ENTITY_CATEGORY_CONFIG
                ),
                cv.Optional(CONF_AREA_ID): select.select_schema(
                    LD600XSelect, entity_category=ENTITY_CATEGORY_CONFIG
                ),
            }
        )
    ).extend(extra or {})


async def select_to_code(
    config: ConfigType, hub_id_key: str, *, area_kinds: tuple[str, ...]
) -> None:
    hub: MockObj = await cg.get_variable(config[hub_id_key])
    selects: SubEntities = select.sub_selects(config, parent=hub)
    await selects(
        CONF_SENSITIVITY,
        hub.set_sensitivity_select,
        SelectType.SELECT_SENSITIVITY,
        options=["low", "medium", "high"],
    )
    await selects(
        CONF_TRIGGER_SPEED,
        hub.set_trigger_speed_select,
        SelectType.SELECT_TRIGGER_SPEED,
        options=["slow", "medium", "fast"],
    )
    await selects(
        CONF_INSTALLATION_MODE,
        hub.set_installation_select,
        SelectType.SELECT_INSTALLATION_MODE,
        options=["top", "side"],
    )
    await selects(
        CONF_AREA_ID,
        hub.set_area_id_select,
        SelectType.SELECT_AREA_ID,
        options=area_id_options(area_kinds),
    )


LD600XSwitch = ld600x_ns.class_("LD600XSwitch", switch.Switch)


SwitchType = ld600x_ns.enum("SwitchType")


def switch_schema(
    hub_class: MockObjClass, hub_id_key: str, *, keys: tuple[str, ...]
) -> cv.Schema:
    schema: cv.Schema = cv.Schema(
        {
            cv.GenerateID(hub_id_key): cv.use_id(hub_class),
            cv.Optional(CONF_LOW_POWER): switch.switch_schema(
                LD600XSwitch,
                block_inverted=True,
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_POINT_CLOUD): switch.switch_schema(
                LD600XSwitch,
                block_inverted=True,
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_TARGET_DISPLAY): switch.switch_schema(
                LD600XSwitch,
                block_inverted=True,
                device_class=DEVICE_CLASS_SWITCH,
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="RESTORE_DEFAULT_ON",
            ),
        }
    )
    return cv.Schema(
        {
            key: value
            for key, value in schema.schema.items()
            if key.schema == hub_id_key or key.schema in keys
        }
    )


async def switch_to_code(
    config: ConfigType, hub_id_key: str, *, keys: tuple[str, ...]
) -> None:
    hub: MockObj = await cg.get_variable(config[hub_id_key])

    switches: SubEntities = switch.sub_switches(config, parent=hub)
    for key, switch_type, setter in (
        (CONF_LOW_POWER, SwitchType.SWITCH_LOW_POWER, hub.set_low_power_switch),
        (CONF_POINT_CLOUD, SwitchType.SWITCH_POINT_CLOUD, hub.set_point_cloud_switch),
        (
            CONF_TARGET_DISPLAY,
            SwitchType.SWITCH_TARGET_DISPLAY,
            hub.set_target_display_switch,
        ),
    ):
        if key in keys:
            await switches(key, setter, switch_type)


LD600XButton = ld600x_ns.class_("LD600XButton", button.Button)


ButtonType = ld600x_ns.enum("ButtonType")


def button_schema(
    hub_class: MockObjClass, hub_id_key: str, *, keys: tuple[str, ...]
) -> cv.Schema:
    schema: cv.Schema = cv.Schema(
        {
            cv.GenerateID(hub_id_key): cv.use_id(hub_class),
            cv.Optional(CONF_APPLY_AREA): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_CONFIG
            ),
            cv.Optional(CONF_AUTO_INTERFERENCE): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_CONFIG
            ),
            cv.Optional(CONF_GET_AREAS): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_CLEAR_INTERFERENCE): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_CONFIG
            ),
            cv.Optional(CONF_RESET_DETECTION_AREA): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_CONFIG
            ),
            cv.Optional(CONF_GET_DELAY): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_GET_SENSITIVITY): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_GET_TRIGGER_SPEED): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_GET_Z_RANGE): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_GET_INSTALLATION): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_GET_LOW_POWER_MODE): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_GET_LOW_POWER_SLEEP_TIME): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
            cv.Optional(CONF_RESET_UNATTENDED): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_CONFIG
            ),
            cv.Optional(CONF_WAKE): button.button_schema(
                LD600XButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
            ),
        }
    )
    return cv.Schema(
        {
            key: value
            for key, value in schema.schema.items()
            if key.schema == hub_id_key or key.schema in keys
        }
    )


def button_final_validate(
    hub_id_key: str, component_name: str, *, wake_key: str | None = None
) -> Callable[[ConfigType], None]:
    def final_validate(config: ConfigType) -> None:
        full_config: fv.FinalValidateConfig = fv.full_config.get()
        hub_id: ID = config[hub_id_key]

        if config.get(CONF_APPLY_AREA):
            has_area_id_select: bool = any(
                entry.get(hub_id_key) == hub_id and entry.get(CONF_AREA_ID) is not None
                for entry in full_config.get("select", [])
            )
            if not has_area_id_select:
                raise cv.Invalid(
                    f"{CONF_APPLY_AREA} requires select.area_id for the same {component_name} instance",
                    path=[CONF_APPLY_AREA],
                )

        if wake_key is not None and config.get(wake_key):
            hub_path: tuple = full_config.get_path_for_id(hub_id)
            hub_config: ConfigType = full_config.get_config_for_path(hub_path[:-1])
            if hub_config.get(CONF_WAKEUP_PIN) is None:
                raise cv.Invalid(
                    f"{wake_key} requires {CONF_WAKEUP_PIN} on the parent {component_name} component",
                    path=[wake_key],
                )

    return final_validate


BUTTON_MAP = {
    CONF_APPLY_AREA: ButtonType.BUTTON_APPLY_AREA,
    CONF_AUTO_INTERFERENCE: ButtonType.BUTTON_AUTO_INTERFERENCE,
    CONF_GET_AREAS: ButtonType.BUTTON_GET_AREAS,
    CONF_CLEAR_INTERFERENCE: ButtonType.BUTTON_CLEAR_INTERFERENCE,
    CONF_RESET_DETECTION_AREA: ButtonType.BUTTON_RESET_DETECTION_AREA,
    CONF_GET_DELAY: ButtonType.BUTTON_GET_DELAY,
    CONF_GET_SENSITIVITY: ButtonType.BUTTON_GET_SENSITIVITY,
    CONF_GET_TRIGGER_SPEED: ButtonType.BUTTON_GET_TRIGGER_SPEED,
    CONF_GET_Z_RANGE: ButtonType.BUTTON_GET_Z_RANGE,
    CONF_GET_INSTALLATION: ButtonType.BUTTON_GET_INSTALLATION,
    CONF_GET_LOW_POWER_MODE: ButtonType.BUTTON_GET_LOW_POWER_MODE,
    CONF_GET_LOW_POWER_SLEEP_TIME: ButtonType.BUTTON_GET_LOW_POWER_SLEEP_TIME,
    CONF_RESET_UNATTENDED: ButtonType.BUTTON_RESET_UNATTENDED,
    CONF_WAKE: ButtonType.BUTTON_WAKE,
}


async def button_to_code(config: ConfigType, hub_id_key: str) -> None:
    for key, button_type in BUTTON_MAP.items():
        if button_config := config.get(key):
            b: MockObj = cg.new_Pvariable(button_config[CONF_ID], button_type)
            await button.register_button(b, button_config)
            await cg.register_parented(b, config[hub_id_key])


def area_id_options(area_kinds: tuple[str, ...]) -> list[str]:
    return [f"{kind}_area_{i}" for kind in area_kinds for i in range(AREA_COUNT)]
