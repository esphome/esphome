from dataclasses import dataclass

from esphome.automation import parent_ref
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
    key: str
    device_class: str = cv.UNDEFINED


_FIELDS = (
    _StatusBit(
        key="ts", register=REG_STAT0, shift=7, device_class=DEVICE_CLASS_OPENING
    ),
    # CHG_STAT: constant current or constant voltage, then charge done
    _StatusBit(
        key="charging",
        register=REG_STAT0,
        shift=5,
        mask=0x03,
        on=(1, 2),
        device_class=DEVICE_CLASS_BATTERY_CHARGING,
    ),
    _StatusBit(key="charge_done", register=REG_STAT0, shift=5, mask=0x03, on=(3,)),
    _StatusBit(
        key="ilim", register=REG_STAT0, shift=4, device_class=DEVICE_CLASS_RUNNING
    ),
    _StatusBit(
        key="vdppm", register=REG_STAT0, shift=3, device_class=DEVICE_CLASS_RUNNING
    ),
    _StatusBit(
        key="vindpm", register=REG_STAT0, shift=2, device_class=DEVICE_CLASS_RUNNING
    ),
    _StatusBit(
        key="thermreg", register=REG_STAT0, shift=1, device_class=DEVICE_CLASS_RUNNING
    ),
    _StatusBit(key="power_good", register=REG_STAT0, device_class=DEVICE_CLASS_PLUG),
    _StatusBit(
        key="vin_ovp", register=REG_STAT1, shift=7, device_class=DEVICE_CLASS_RUNNING
    ),
    _StatusBit(
        key="battery_uvlo",
        register=REG_STAT1,
        shift=6,
        device_class=DEVICE_CLASS_RUNNING,
    ),
    _StatusBit(key="safety_timer_flag", register=REG_STAT1, shift=2),
    _StatusBit(key="wake1_flag", register=REG_STAT1, shift=1),
    _StatusBit(key="wake2_flag", register=REG_STAT1),
    _StatusBit(key="ts_flag", register=REG_FLAG0, shift=7),
    _StatusBit(key="ilim_flag", register=REG_FLAG0, shift=6),
    _StatusBit(key="vdppm_flag", register=REG_FLAG0, shift=5),
    _StatusBit(key="vindpm_flag", register=REG_FLAG0, shift=4),
    _StatusBit(key="thermreg_flag", register=REG_FLAG0, shift=3),
    _StatusBit(key="vin_ovp_flag", register=REG_FLAG0, shift=2),
    _StatusBit(key="battery_uvlo_flag", register=REG_FLAG0, shift=1),
    _StatusBit(key="battery_ocp_flag", register=REG_FLAG0),
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
    sensors = binary_sensor.sub_binary_sensors(config)
    for field in _FIELDS:
        if (var := await sensors(field.key, None)) is not None:
            # ::bs->publish_state(((status[0] >> 7) & 0x01));
            STATUS_PUBLISHER.add(
                hub, parent_ref(var).publish_state(field.is_on(STATUS))
            )
