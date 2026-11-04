from dataclasses import dataclass

from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_BATTERY_CHARGING,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_PROBLEM,
)
from esphome.cpp_generator import HexIntLiteral
from esphome.types import ConfigType

from .. import (
    CHARGER_STATUS_1,
    CHARGER_STATUS_2,
    CHARGER_STATUS_3,
    CONF_BQ25820_ID,
    DUMP,
    DUMP_TAG,
    FAULT_STATUS,
    STATUS,
    STATUS_PUBLISHER,
    BQ25820BaseSchema,
)

DEPENDENCIES = ["bq25820"]
_BINARY_SENSOR_TYPE = cg.RawExpression('LOG_STR_LITERAL("Binary Sensor")')

# Bit set for each field value that reads as on: a single bit is on when it is 1
_BIT_SET = 0b10


@dataclass(frozen=True, kw_only=True)
class _StatusBit:
    """On when the field (status[register] >> shift) & mask is a set bit of on_values."""

    key: str
    register: int
    shift: int = 0
    mask: int = 0x01
    on_values: int = _BIT_SET
    device_class: str = cv.UNDEFINED


_FIELDS = (
    # CHARGE_STAT: trickle to top-off, done, trickle or precharge (below VBAT_LOWV)
    _StatusBit(
        key="charging",
        register=CHARGER_STATUS_1,
        mask=0x07,
        on_values=0b01111110,
        device_class=DEVICE_CLASS_BATTERY_CHARGING,
    ),
    _StatusBit(
        key="charged", register=CHARGER_STATUS_1, mask=0x07, on_values=0b10000000
    ),
    _StatusBit(
        key="low_battery",
        register=CHARGER_STATUS_1,
        mask=0x07,
        on_values=0b00000110,
        device_class=DEVICE_CLASS_BATTERY,
    ),
    _StatusBit(
        key="pg", register=CHARGER_STATUS_2, shift=7, device_class=DEVICE_CLASS_POWER
    ),
    _StatusBit(key="reverse", register=CHARGER_STATUS_3, shift=2),
    # FAULT_STATUS bits 7 to 1
    *(
        _StatusBit(
            key=key,
            register=FAULT_STATUS,
            shift=shift,
            device_class=DEVICE_CLASS_PROBLEM,
        )
        for shift, key in zip(
            range(7, 0, -1),
            (
                "vac_undervoltage",
                "vac_overvoltage",
                "ibat_overcurrent",
                "vbat_overvoltage",
                "thermal_shutdown",
                "charge_safety_timer",
                "drv_supply_invalid",
            ),
            strict=True,
        )
    ),
)


def _state_expression(reg: int, shift: int, mask: int, match: int) -> cg.MockObj:
    """Whether the field (status[reg] >> shift) & mask reads as on, as a bit of match."""
    value = (STATUS[reg] >> shift) & HexIntLiteral(mask)
    if mask == 0x01 and match == _BIT_SET:
        return value
    return (cg.MockObj(f"{match:#06b}") >> value) & 1


CONFIG_SCHEMA = cv.All(
    BQ25820BaseSchema.extend(
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
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    for field in _FIELDS:
        if (var := await sensors(field.key, None)) is not None:
            state = _state_expression(
                field.register, field.shift, field.mask, field.on_values
            )
            STATUS_PUBLISHER.add(hub, parent_ref(var).publish_state(state))
            DUMP.add(
                hub,
                binary_sensor.binary_sensor_ns.log_binary_sensor(
                    DUMP_TAG, "  ", _BINARY_SENSOR_TYPE, parent_ref(var)
                ),
            )
