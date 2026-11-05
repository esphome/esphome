from dataclasses import dataclass

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_OPENING,
    DEVICE_CLASS_PLUG,
    DEVICE_CLASS_RUNNING,
)
from esphome.types import ConfigType

from . import BQ25186_CHILD_SCHEMA, CONF_BQ25186_ID, STATUS, STATUS_PUBLISHER

DEPENDENCIES = ["bq25186"]

REG_STAT0 = 0
REG_STAT1 = 1
REG_FLAG0 = 2


@dataclass(frozen=True, kw_only=True)
class _StatusBit(cg.RegisterField):
    """On when the field is a set bit of on_values."""

    key: str
    on_values: int = cg.BIT_FIELD_SET
    device_class: str = cv.UNDEFINED


_FIELDS = (
    _StatusBit(REG_STAT0, 7, key="ts", device_class=DEVICE_CLASS_OPENING),
    _StatusBit(
        REG_STAT0,
        5,
        0x03,
        key="charging",
        on_values=0b0110,
        device_class=DEVICE_CLASS_BATTERY_CHARGING,
    ),
    _StatusBit(REG_STAT0, 5, 0x03, key="charge_done", on_values=0b1000),
    _StatusBit(REG_STAT0, 4, key="ilim", device_class=DEVICE_CLASS_RUNNING),
    _StatusBit(REG_STAT0, 3, key="vdppm", device_class=DEVICE_CLASS_RUNNING),
    _StatusBit(REG_STAT0, 2, key="vindpm", device_class=DEVICE_CLASS_RUNNING),
    _StatusBit(REG_STAT0, 1, key="thermreg", device_class=DEVICE_CLASS_RUNNING),
    _StatusBit(REG_STAT0, 0, key="power_good", device_class=DEVICE_CLASS_PLUG),
    _StatusBit(REG_STAT1, 7, key="vin_ovp", device_class=DEVICE_CLASS_RUNNING),
    _StatusBit(REG_STAT1, 6, key="battery_uvlo", device_class=DEVICE_CLASS_RUNNING),
    _StatusBit(REG_STAT1, 2, key="safety_timer_flag"),
    _StatusBit(REG_STAT1, 1, key="wake1_flag"),
    _StatusBit(REG_STAT1, 0, key="wake2_flag"),
    _StatusBit(REG_FLAG0, 7, key="ts_flag"),
    _StatusBit(REG_FLAG0, 6, key="ilim_flag"),
    _StatusBit(REG_FLAG0, 5, key="vdppm_flag"),
    _StatusBit(REG_FLAG0, 4, key="vindpm_flag"),
    _StatusBit(REG_FLAG0, 3, key="thermreg_flag"),
    _StatusBit(REG_FLAG0, 2, key="vin_ovp_flag"),
    _StatusBit(REG_FLAG0, 1, key="battery_uvlo_flag"),
    _StatusBit(REG_FLAG0, 0, key="battery_ocp_flag"),
)


CONFIG_SCHEMA = cv.All(
    BQ25186_CHILD_SCHEMA.extend(
        {
            cv.Optional(field.key): binary_sensor.binary_sensor_schema(
                device_class=field.device_class
            )
            for field in _FIELDS
        }
    ),
    cv.has_at_least_one_key(*(field.key for field in _FIELDS)),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25186_ID])
    await STATUS_PUBLISHER.publish_children(
        hub,
        binary_sensor.sub_binary_sensors(config),
        {field.key: field.is_on(STATUS, field.on_values) for field in _FIELDS},
    )
