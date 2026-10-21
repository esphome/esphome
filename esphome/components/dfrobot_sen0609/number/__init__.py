from typing import NamedTuple

import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAX_RANGE,
    CONF_MIN_RANGE,
    CONF_MODEL,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_DURATION,
    ENTITY_CATEGORY_CONFIG,
    ICON_MOTION_SENSOR,
    ICON_RULER,
    ICON_TIMELAPSE,
    UNIT_METER,
    UNIT_SECOND,
)
from esphome.core import CORE
from esphome.types import ConfigType

from .. import (
    CONF_DFROBOT_SEN0609_ID,
    MAX_RANGES,
    MODE_PRESENCE,
    MODE_SPEED_AND_DISTANCE,
    DFRobotSEN0609Component,
    dfrobot_sen0609_ns,
    final_validate_mode,
    get_hub_config,
)

DFRobotSEN0609Number = dfrobot_sen0609_ns.class_("DFRobotSEN0609Number", number.Number)
NumberSetting = dfrobot_sen0609_ns.enum("NumberSetting")

CONF_HOLD_SENSITIVITY = "hold_sensitivity"
CONF_INHIBIT_TIME = "inhibit_time"
CONF_OFF_LATENCY = "off_latency"
CONF_ON_LATENCY = "on_latency"
CONF_THRESHOLD_FACTOR = "threshold_factor"
CONF_TRIGGER_RANGE = "trigger_range"
CONF_TRIGGER_SENSITIVITY = "trigger_sensitivity"


class Setting(NamedTuple):
    key: str
    setting: cg.MockObj
    min_value: float
    max_value: float | None  # None: the model's longest range
    step: float
    mode: str | None  # None: both modes
    schema: cv.Schema


def _distance(icon: str = ICON_RULER) -> cv.Schema:
    return number.number_schema(
        DFRobotSEN0609Number,
        device_class=DEVICE_CLASS_DISTANCE,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=icon,
        unit_of_measurement=UNIT_METER,
    )


def _duration() -> cv.Schema:
    return number.number_schema(
        DFRobotSEN0609Number,
        device_class=DEVICE_CLASS_DURATION,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_TIMELAPSE,
        unit_of_measurement=UNIT_SECOND,
    )


def _plain() -> cv.Schema:
    return number.number_schema(
        DFRobotSEN0609Number,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_MOTION_SENSOR,
    )


SETTINGS = (
    Setting(
        CONF_MIN_RANGE,
        NumberSetting.NUMBER_MIN_RANGE,
        0.6,
        None,
        0.1,
        None,
        _distance(),
    ),
    Setting(
        CONF_MAX_RANGE,
        NumberSetting.NUMBER_MAX_RANGE,
        0.6,
        None,
        0.1,
        None,
        _distance(),
    ),
    Setting(
        CONF_TRIGGER_RANGE,
        NumberSetting.NUMBER_TRIGGER_RANGE,
        0.6,
        None,
        0.1,
        MODE_PRESENCE,
        _distance(ICON_MOTION_SENSOR),
    ),
    Setting(
        CONF_HOLD_SENSITIVITY,
        NumberSetting.NUMBER_HOLD_SENSITIVITY,
        0,
        9,
        1,
        MODE_PRESENCE,
        _plain(),
    ),
    Setting(
        CONF_TRIGGER_SENSITIVITY,
        NumberSetting.NUMBER_TRIGGER_SENSITIVITY,
        0,
        9,
        1,
        MODE_PRESENCE,
        _plain(),
    ),
    Setting(
        CONF_ON_LATENCY,
        NumberSetting.NUMBER_ON_LATENCY,
        0,
        2,
        0.01,
        MODE_PRESENCE,
        _duration(),
    ),
    Setting(
        CONF_OFF_LATENCY,
        NumberSetting.NUMBER_OFF_LATENCY,
        2,
        1500,
        0.5,
        MODE_PRESENCE,
        _duration(),
    ),
    Setting(
        CONF_INHIBIT_TIME,
        NumberSetting.NUMBER_INHIBIT_TIME,
        0.1,
        60,
        0.1,
        MODE_PRESENCE,
        _duration(),
    ),
    Setting(
        CONF_THRESHOLD_FACTOR,
        NumberSetting.NUMBER_THRESHOLD_FACTOR,
        0,
        65535,
        1,
        MODE_SPEED_AND_DISTANCE,
        _plain(),
    ),
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_DFROBOT_SEN0609_ID): cv.use_id(DFRobotSEN0609Component),
        **{cv.Optional(s.key): s.schema for s in SETTINGS},
    }
)

FINAL_VALIDATE_SCHEMA = final_validate_mode(
    {s.key: s.mode for s in SETTINGS if s.mode is not None}
)


async def to_code(config: ConfigType) -> None:
    hub_id = config[CONF_DFROBOT_SEN0609_ID]
    hub = await cg.get_variable(hub_id)
    max_range = MAX_RANGES[get_hub_config(CORE.config, hub_id)[CONF_MODEL]]
    numbers = number.sub_numbers(config, parent=hub)
    for s in SETTINGS:
        await numbers(
            s.key,
            lambda n, setting=s.setting: hub.set_number(setting, n),
            s.setting,
            min_value=s.min_value,
            max_value=max_range if s.max_value is None else s.max_value,
            step=s.step,
        )
