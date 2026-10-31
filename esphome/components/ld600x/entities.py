from collections.abc import Callable
from typing import Any

from esphome.automation import parent_ref
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
    CONF_ID,
    CONF_PLATFORM,
    CONF_SENSITIVITY,
    CONF_TARGET,
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
from esphome.cpp_generator import MockObj, MockObjClass, ParameterListExpression
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import AreaAxis, AreaKind, ButtonType, request_model_sizes
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
    CONF_LOW_POWER_SLEEP_TIME,
    CONF_OTA_VERSION,
    CONF_POINT_CLOUD,
    CONF_POINT_COUNT,
    CONF_RESET_DETECTION_AREA,
    CONF_RESET_UNATTENDED,
    CONF_TARGET_DISPLAY,
    CONF_TRIGGER_SPEED,
    CONF_WORK_MODE,
    CONF_Z,
    CONF_Z_MAX,
    CONF_Z_MIN,
    KEY_X_MAX,
    KEY_X_MIN,
    KEY_Y_MAX,
    KEY_Y_MIN,
    MAX_TARGETS,
)

_AREA_KINDS = {
    "interference": AreaKind.AREA_KIND_INTERFERENCE,
    "detection": AreaKind.AREA_KIND_DETECTION,
    "dwell": AreaKind.AREA_KIND_DWELL,
}

_AREA_AXES = (
    (KEY_X_MIN, AreaAxis.AREA_AXIS_X_MIN),
    (KEY_X_MAX, AreaAxis.AREA_AXIS_X_MAX),
    (KEY_Y_MIN, AreaAxis.AREA_AXIS_Y_MIN),
    (KEY_Y_MAX, AreaAxis.AREA_AXIS_Y_MAX),
    (CONF_Z_MIN, AreaAxis.AREA_AXIS_Z_MIN),
    (CONF_Z_MAX, AreaAxis.AREA_AXIS_Z_MAX),
)

_METER_SENSOR_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_METER,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_DISTANCE,
    state_class=STATE_CLASS_MEASUREMENT,
)

_TARGET_AXIS_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_METER,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_DISTANCE,
    filters=sensor.TIMEOUT_THROTTLE_FILTERS,
    state_class=STATE_CLASS_MEASUREMENT,
)

_METER_NUMBER_SCHEMA = number.number_schema(
    number.ApplyNumber,
    unit_of_measurement=UNIT_METER,
    device_class=DEVICE_CLASS_DISTANCE,
    entity_category=ENTITY_CATEGORY_CONFIG,
)


TARGET_SCHEMA = cv.Schema(
    {
        **{cv.Optional(key): _TARGET_AXIS_SCHEMA for key in (CONF_X, CONF_Y, CONF_Z)},
        cv.Optional(CONF_DOPPLER_INDEX): sensor.sensor_schema(
            accuracy_decimals=0,
            filters=sensor.TIMEOUT_THROTTLE_FILTERS,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_CLUSTER_ID): sensor.sensor_schema(
            accuracy_decimals=0,
        ),
    }
)


AREA_SCHEMA = cv.Schema(
    {cv.Optional(key): _METER_SENSOR_SCHEMA for key, _ in _AREA_AXES}
)

_TARGET_KEYS = tuple(f"target_{i + 1}" for i in range(MAX_TARGETS))
_DETECTION_AREA_KEYS = tuple(f"detection_area_{i}" for i in range(AREA_COUNT))


def _hub_has(domain: str, key: str, hub_id_key: str, hub_id: ID) -> bool:
    """Return whether a block of domain on the hub hub_id sets key."""
    return any(
        entry.get(hub_id_key) == hub_id and entry.get(key) is not None
        for entry in fv.full_config.get().get(domain, [])
    )


def sensor_schema(
    hub_class: MockObjClass, hub_id_key: str, *, area_kinds: tuple[str, ...]
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
        .extend({cv.Optional(key): TARGET_SCHEMA for key in _TARGET_KEYS})
        .extend(
            {
                cv.Optional(f"{kind}_area_{i}"): AREA_SCHEMA
                for kind in area_kinds
                for i in range(AREA_COUNT)
            }
        )
    )


async def sensor_to_code(
    config: ConfigType, hub_id_key: str, *, area_kinds: tuple[str, ...]
) -> None:
    hub = await cg.get_variable(config[hub_id_key])

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TARGET_COUNT, hub.set_target_count_sensor)
    await sensors(CONF_POINT_COUNT, hub.set_point_count_sensor)

    target_setters = (
        (CONF_X, hub.set_target_x_sensor),
        (CONF_Y, hub.set_target_y_sensor),
        (CONF_Z, hub.set_target_z_sensor),
        (CONF_DOPPLER_INDEX, hub.set_target_dop_idx_sensor),
        (CONF_CLUSTER_ID, hub.set_target_cluster_id_sensor),
    )
    for i, target_key in enumerate(_TARGET_KEYS):
        if target_config := config.get(target_key):
            for key, setter in target_setters:
                if conf := target_config.get(key):
                    cg.add(setter(i, await sensor.new_sensor(conf)))

    used_kinds = 0
    for kind_index, kind in enumerate(area_kinds):
        area_kind = _AREA_KINDS[kind]
        for i in range(AREA_COUNT):
            if area_config := config.get(f"{kind}_area_{i}"):
                used_kinds = kind_index + 1
                for key, axis in _AREA_AXES:
                    if conf := area_config.get(key):
                        sens = await sensor.new_sensor(conf)
                        cg.add(hub.set_area_sensor(area_kind, i, axis, sens))
    # The hub sizes the common kinds; a later kind such as dwell is sized only when used.
    if used_kinds:
        request_model_sizes(hub, area_kinds=used_kinds)


_OCCUPANCY_SCHEMA = binary_sensor.binary_sensor_schema(
    device_class=DEVICE_CLASS_OCCUPANCY
)


def binary_sensor_schema(hub_class: MockObjClass, hub_id_key: str) -> cv.Schema:
    return cv.Schema(
        {
            cv.GenerateID(hub_id_key): cv.use_id(hub_class),
            cv.Optional(CONF_TARGET): _OCCUPANCY_SCHEMA,
            **{
                cv.Optional(key): _OCCUPANCY_SCHEMA
                for key in (*_TARGET_KEYS, *_DETECTION_AREA_KEYS)
            },
        }
    )


async def binary_sensor_to_code(config: ConfigType, hub_id_key: str) -> None:
    hub = await cg.get_variable(config[hub_id_key])

    sensors = binary_sensor.sub_binary_sensors(config)
    await sensors(CONF_TARGET, hub.set_presence_binary_sensor)

    for keys, setter in (
        (_TARGET_KEYS, hub.set_target_presence_binary_sensor),
        (_DETECTION_AREA_KEYS, hub.set_area_presence_binary_sensor),
    ):
        for i, key in enumerate(keys):
            if conf := config.get(key):
                cg.add(setter(i, await binary_sensor.new_binary_sensor(conf)))


def text_sensor_schema(hub_class: MockObjClass, hub_id_key: str) -> cv.Schema:
    return cv.Schema(
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


async def text_sensor_to_code(config: ConfigType, hub_id_key: str) -> None:
    hub = await cg.get_variable(config[hub_id_key])
    sensors = text_sensor.sub_text_sensors(config)
    await sensors(CONF_WORK_MODE, hub.set_work_mode_text_sensor)
    await sensors(CONF_OTA_VERSION, hub.set_ota_version_text_sensor)


def number_schema(
    hub_class: MockObjClass, hub_id_key: str, *, extra: dict[Any, Any] | None = None
) -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.GenerateID(hub_id_key): cv.use_id(hub_class),
                cv.Optional(CONF_HOLD_DELAY): number.number_schema(
                    number.ApplyNumber,
                    unit_of_measurement=UNIT_SECOND,
                    device_class=DEVICE_CLASS_DURATION,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_Z_MIN): _METER_NUMBER_SCHEMA,
                cv.Optional(CONF_Z_MAX): _METER_NUMBER_SCHEMA,
                cv.Optional(CONF_LOW_POWER_SLEEP_TIME): number.number_schema(
                    number.ApplyNumber,
                    unit_of_measurement=UNIT_MILLISECOND,
                    device_class=DEVICE_CLASS_DURATION,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_AREA_CONFIG): cv.Schema(
                    {cv.Optional(key): _METER_NUMBER_SCHEMA for key, _ in _AREA_AXES}
                ),
            }
        )
    ).extend(extra or {})


def number_final_validate(
    hub_id_key: str, component_name: str
) -> Callable[[ConfigType], None]:
    unique = unique_settings_final_validate(number.DOMAIN, hub_id_key, component_name)

    def final_validate(config: ConfigType) -> None:
        unique(config)
        if config.get(CONF_AREA_CONFIG) is None:
            return

        hub_id: ID = config[hub_id_key]
        if not _hub_has(button.DOMAIN, CONF_APPLY_AREA, hub_id_key, hub_id):
            raise cv.Invalid(
                f"{CONF_AREA_CONFIG} requires button.apply_area for the same {component_name} instance",
                path=[CONF_AREA_CONFIG],
            )
        if not _hub_has(select.DOMAIN, CONF_AREA_ID, hub_id_key, hub_id):
            raise cv.Invalid(
                f"{CONF_AREA_CONFIG} requires select.area_id for the same {component_name} instance",
                path=[CONF_AREA_CONFIG],
            )

    return final_validate


# The apply call picks the setting by entity pointer, so every number shares one generated class.
NUMBER_CONTROL: dict[str, str] = {
    "apply": "set_number_value(entity, value)",
    "publish": "before",
}


async def number_to_code(config: ConfigType, hub_id_key: str) -> SubEntities:
    """Create the shared numbers and return the bound helper for a model's own numbers."""
    hub = await cg.get_variable(config[hub_id_key])
    numbers = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_HOLD_DELAY,
        hub.set_hold_delay_number,
        min_value=0,
        max_value=65535,
        step=1,
        **NUMBER_CONTROL,
    )
    meters: dict[str, Any] = {
        "min_value": -10,
        "max_value": 10,
        "step": 0.1,
        **NUMBER_CONTROL,
    }
    await numbers(CONF_Z_MIN, hub.set_z_min_number, **meters)
    await numbers(CONF_Z_MAX, hub.set_z_max_number, **meters)
    # 0x0205 is a uint32 of ms with no documented maximum; cap it at a minute
    await numbers(
        CONF_LOW_POWER_SLEEP_TIME,
        hub.set_low_power_sleep_number,
        min_value=0,
        max_value=60000,
        step=100,
        **NUMBER_CONTROL,
    )

    if area_config := config.get(CONF_AREA_CONFIG):
        area = number.sub_numbers(area_config, parent=hub)
        await area(KEY_X_MIN, hub.set_area_x_min_number, **meters)
        await area(KEY_X_MAX, hub.set_area_x_max_number, **meters)
        await area(KEY_Y_MIN, hub.set_area_y_min_number, **meters)
        await area(KEY_Y_MAX, hub.set_area_y_max_number, **meters)
        await area(CONF_Z_MIN, hub.set_area_z_min_number, **meters)
        await area(CONF_Z_MAX, hub.set_area_z_max_number, **meters)
    return numbers


def select_schema(
    hub_class: MockObjClass, hub_id_key: str, *, extra: dict[Any, Any] | None = None
) -> cv.Schema:
    return (
        cv.Schema(
            {
                cv.GenerateID(hub_id_key): cv.use_id(hub_class),
                cv.Optional(CONF_SENSITIVITY): select.select_schema(
                    select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
                ),
                cv.Optional(CONF_TRIGGER_SPEED): select.select_schema(
                    select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
                ),
                cv.Optional(CONF_INSTALLATION_MODE): select.select_schema(
                    select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
                ),
                cv.Optional(CONF_AREA_ID): select.select_schema(
                    select.ApplySelect, entity_category=ENTITY_CATEGORY_CONFIG
                ),
            }
        )
    ).extend(extra or {})


SELECT_CONTROL: dict[str, str] = {
    "apply": "set_select_value(entity, index)",
    "publish": "before",
}


async def select_to_code(
    config: ConfigType, hub_id_key: str, *, area_kinds: tuple[str, ...]
) -> SubEntities:
    """Create the shared selects and return the bound helper for a model's own selects."""
    hub = await cg.get_variable(config[hub_id_key])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_SENSITIVITY,
        hub.set_sensitivity_select,
        options=["low", "medium", "high"],
        **SELECT_CONTROL,
    )
    await selects(
        CONF_TRIGGER_SPEED,
        hub.set_trigger_speed_select,
        options=["slow", "medium", "fast"],
        **SELECT_CONTROL,
    )
    await selects(
        CONF_INSTALLATION_MODE,
        hub.set_installation_select,
        options=["top", "side"],
        **SELECT_CONTROL,
    )
    await selects(
        CONF_AREA_ID,
        hub.set_area_id_select,
        options=area_id_options(area_kinds),
        **SELECT_CONTROL,
    )
    return selects


def _config_switch_schema(**kwargs: Any) -> cv.Schema:
    return switch.switch_schema(
        switch.ApplySwitch,
        block_inverted=True,
        device_class=DEVICE_CLASS_SWITCH,
        entity_category=ENTITY_CATEGORY_CONFIG,
        **kwargs,
    )


CONFIG_SWITCH_SCHEMA = _config_switch_schema()

_SWITCH_SCHEMAS = {
    CONF_POINT_CLOUD: CONFIG_SWITCH_SCHEMA,
    CONF_TARGET_DISPLAY: _config_switch_schema(
        default_restore_mode="RESTORE_DEFAULT_ON"
    ),
}


def switch_schema(
    hub_class: MockObjClass,
    hub_id_key: str,
    *,
    extra: dict[Any, Any] | None = None,
) -> cv.Schema:
    return cv.Schema(
        {
            cv.GenerateID(hub_id_key): cv.use_id(hub_class),
            **{cv.Optional(key): schema for key, schema in _SWITCH_SCHEMAS.items()},
        }
    ).extend(extra or {})


SWITCH_CONTROL: dict[str, str] = {
    "apply": "set_switch_state(entity, state)",
    # A full command queue rejects the write, and the switch then keeps its old state.
    "publish": "on_success",
}


async def switch_to_code(config: ConfigType, hub_id_key: str) -> None:
    hub = await cg.get_variable(config[hub_id_key])
    switches = switch.sub_switches(config, parent=hub)
    await switches(CONF_POINT_CLOUD, hub.set_point_cloud_switch, **SWITCH_CONTROL)
    await switches(CONF_TARGET_DISPLAY, hub.set_target_display_switch, **SWITCH_CONTROL)


CONFIG_BUTTON_SCHEMA = button.button_schema(
    button.ApplyButton, entity_category=ENTITY_CATEGORY_CONFIG
)
DIAGNOSTIC_BUTTON_SCHEMA = button.button_schema(
    button.ApplyButton, entity_category=ENTITY_CATEGORY_DIAGNOSTIC
)
_BUTTON_SCHEMAS = {
    CONF_APPLY_AREA: CONFIG_BUTTON_SCHEMA,
    CONF_AUTO_INTERFERENCE: CONFIG_BUTTON_SCHEMA,
    CONF_GET_AREAS: DIAGNOSTIC_BUTTON_SCHEMA,
    CONF_CLEAR_INTERFERENCE: CONFIG_BUTTON_SCHEMA,
    CONF_RESET_DETECTION_AREA: CONFIG_BUTTON_SCHEMA,
    CONF_GET_DELAY: DIAGNOSTIC_BUTTON_SCHEMA,
    CONF_GET_SENSITIVITY: DIAGNOSTIC_BUTTON_SCHEMA,
    CONF_GET_TRIGGER_SPEED: DIAGNOSTIC_BUTTON_SCHEMA,
    CONF_GET_Z_RANGE: DIAGNOSTIC_BUTTON_SCHEMA,
    CONF_GET_INSTALLATION: DIAGNOSTIC_BUTTON_SCHEMA,
    CONF_GET_LOW_POWER_MODE: DIAGNOSTIC_BUTTON_SCHEMA,
    CONF_GET_LOW_POWER_SLEEP_TIME: DIAGNOSTIC_BUTTON_SCHEMA,
    CONF_RESET_UNATTENDED: CONFIG_BUTTON_SCHEMA,
}


def button_schema(
    hub_class: MockObjClass,
    hub_id_key: str,
    *,
    extra: dict[Any, Any] | None = None,
) -> cv.Schema:
    return cv.Schema(
        {
            cv.GenerateID(hub_id_key): cv.use_id(hub_class),
            **{cv.Optional(key): schema for key, schema in _BUTTON_SCHEMAS.items()},
        }
    ).extend(extra or {})


def button_final_validate(
    hub_id_key: str, component_name: str
) -> Callable[[ConfigType], None]:
    def final_validate(config: ConfigType) -> None:
        hub_id: ID = config[hub_id_key]

        if config.get(CONF_APPLY_AREA) and not _hub_has(
            select.DOMAIN, CONF_AREA_ID, hub_id_key, hub_id
        ):
            raise cv.Invalid(
                f"{CONF_APPLY_AREA} requires select.area_id for the same {component_name} instance",
                path=[CONF_APPLY_AREA],
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
}


async def button_to_code(
    config: ConfigType,
    hub_id_key: str,
    *,
    extra: dict[str, MockObj] | None = None,
) -> None:
    extra = extra or {}
    confs: list[tuple[MockObj, ConfigType]] = [
        (button_type, conf)
        for key, button_type in {**BUTTON_MAP, **extra}.items()
        if (conf := config.get(key)) is not None
    ]
    if not confs:
        return
    if any(key in config for key in extra):
        # Two enum types in one ?: chain draw a warning, so a model's buttons put everything in uint8_t.
        confs = [
            (cg.RawExpression(f"static_cast<uint8_t>({button_type})"), conf)
            for button_type, conf in confs
        ]
    hub = await cg.get_variable(config[hub_id_key])
    params = button.APPLY_ENTITY.params
    # One function for all buttons, picking the type by pointer: one class instead of one per button.
    # Named after the first button so two button blocks on one hub do not clash.
    fn = f"esphome__{confs[0][1][CONF_ID].id}__button_fn"
    cg.add_global(
        cg.RawStatement(f"static void {fn}({ParameterListExpression(*params)});")
    )
    template = cg.TemplateArguments(cg.RawExpression(fn))
    created: list[tuple[MockObj, MockObj]] = [
        (button_type, await button.new_button(conf, template))
        for button_type, conf in confs
    ]
    *head, (last_type, _) = created
    chain = "".join(f"entity == {parent_ref(var)} ? {t} : " for t, var in head)
    cg.static_function(
        fn,
        cg.void,
        list(params),
        [
            f"const auto type = {chain}{last_type};",
            f"{parent_ref(hub)}->press_button(type);",
        ],
    )


def unique_settings_final_validate(
    domain: str, hub_id_key: str, component_name: str
) -> Callable[[ConfigType], None]:
    """Reject a setting set in two platform blocks of one hub, which keeps one entity per setting."""

    def final_validate(config: ConfigType) -> None:
        hub_id: ID = config[hub_id_key]
        entries: list[ConfigType] = [
            entry
            for entry in fv.full_config.get().get(domain, [])
            if entry.get(CONF_PLATFORM) == component_name
            and entry.get(hub_id_key) == hub_id
        ]
        for key, value in config.items():
            if key in (CONF_PLATFORM, hub_id_key):
                continue
            # area_config groups six settings, so each one is checked on its own
            for path in (
                [[key, sub] for sub in value] if key == CONF_AREA_CONFIG else [[key]]
            ):
                if sum(_has_path(entry, path) for entry in entries) > 1:
                    raise cv.Invalid(
                        f"{'.'.join(path)} is already set for this {component_name} in another {domain} block",
                        path=path,
                    )

    return final_validate


def _has_path(entry: ConfigType, path: list[str]) -> bool:
    for key in path:
        if key not in entry:
            return False
        entry = entry[key]
    return True


def area_id_options(area_kinds: tuple[str, ...]) -> list[str]:
    return [f"{kind}_area_{i}" for kind in area_kinds for i in range(AREA_COUNT)]
