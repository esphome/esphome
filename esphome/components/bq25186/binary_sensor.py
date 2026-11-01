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
from esphome.cpp_generator import HexIntLiteral
from esphome.types import ConfigType

from . import BQ25186_CHILD_SCHEMA, CONF_BQ25186_ID, STATUS, STATUS_PUBLISHER

DEPENDENCIES = ["bq25186"]

REG_STAT0 = 0
REG_STAT1 = 1
REG_FLAG0 = 2
# Bit set for each field value that reads as on: a single bit is on when it is 1
_BIT_SET = 0b10

# (key, status register, bit shift, field mask, on-values bitmap, device class)
_FIELDS = (
    ("ts", REG_STAT0, 7, 0x01, _BIT_SET, DEVICE_CLASS_OPENING),
    ("charging", REG_STAT0, 5, 0x03, 0b0110, DEVICE_CLASS_BATTERY_CHARGING),
    ("charge_done", REG_STAT0, 5, 0x03, 0b1000, cv.UNDEFINED),
    ("ilim", REG_STAT0, 4, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("vdppm", REG_STAT0, 3, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("vindpm", REG_STAT0, 2, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("thermreg", REG_STAT0, 1, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("power_good", REG_STAT0, 0, 0x01, _BIT_SET, DEVICE_CLASS_PLUG),
    ("vin_ovp", REG_STAT1, 7, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("battery_uvlo", REG_STAT1, 6, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("safety_timer_flag", REG_STAT1, 2, 0x01, _BIT_SET, cv.UNDEFINED),
    ("wake1_flag", REG_STAT1, 1, 0x01, _BIT_SET, cv.UNDEFINED),
    ("wake2_flag", REG_STAT1, 0, 0x01, _BIT_SET, cv.UNDEFINED),
    ("ts_flag", REG_FLAG0, 7, 0x01, _BIT_SET, cv.UNDEFINED),
    ("ilim_flag", REG_FLAG0, 6, 0x01, _BIT_SET, cv.UNDEFINED),
    ("vdppm_flag", REG_FLAG0, 5, 0x01, _BIT_SET, cv.UNDEFINED),
    ("vindpm_flag", REG_FLAG0, 4, 0x01, _BIT_SET, cv.UNDEFINED),
    ("thermreg_flag", REG_FLAG0, 3, 0x01, _BIT_SET, cv.UNDEFINED),
    ("vin_ovp_flag", REG_FLAG0, 2, 0x01, _BIT_SET, cv.UNDEFINED),
    ("battery_uvlo_flag", REG_FLAG0, 1, 0x01, _BIT_SET, cv.UNDEFINED),
    ("battery_ocp_flag", REG_FLAG0, 0, 0x01, _BIT_SET, cv.UNDEFINED),
)


def _state_expression(reg: int, shift: int, mask: int, match: int) -> cg.MockObj:
    """Whether the field (status[reg] >> shift) & mask reads as on, as a bit of match."""
    value = (STATUS[reg] >> shift) & HexIntLiteral(mask)
    if mask == 0x01 and match == _BIT_SET:
        return value
    return (cg.MockObj(f"{match:#06b}") >> value) & 1


CONFIG_SCHEMA = cv.All(
    BQ25186_CHILD_SCHEMA.extend(
        {
            cv.Optional(key): binary_sensor.binary_sensor_schema(
                device_class=device_class
            )
            for key, *_, device_class in _FIELDS
        }
    ),
    cv.has_at_least_one_key(*(key for key, *_ in _FIELDS)),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25186_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    for key, reg, shift, mask, match, _ in _FIELDS:
        if (var := await sensors(key, None)) is not None:
            state = _state_expression(reg, shift, mask, match)
            STATUS_PUBLISHER.add(hub, parent_ref(var).publish_state(state))
