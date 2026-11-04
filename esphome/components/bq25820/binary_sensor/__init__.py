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
    DUMP_STATUS,
    FAULT_STATUS,
    STATUS,
    STATUS_PUBLISHER,
    BQ25820BaseSchema,
    bq25820_ns,
)

DEPENDENCIES = ["bq25820"]

# Bit set for each field value that reads as on: a single bit is on when it is 1
_BIT_SET = 0b10

# (key, status register, bit shift, field mask, on-values bitmap, device class)
_FIELDS = (
    # CHARGE_STAT: trickle to top-off, done, trickle or precharge (below VBAT_LOWV)
    (
        "charging",
        CHARGER_STATUS_1,
        0,
        0x07,
        0b01111110,
        DEVICE_CLASS_BATTERY_CHARGING,
    ),
    ("charged", CHARGER_STATUS_1, 0, 0x07, 0b10000000, cv.UNDEFINED),
    ("low_battery", CHARGER_STATUS_1, 0, 0x07, 0b00000110, DEVICE_CLASS_BATTERY),
    ("pg", CHARGER_STATUS_2, 7, 0x01, _BIT_SET, DEVICE_CLASS_POWER),
    ("reverse", CHARGER_STATUS_3, 2, 0x01, _BIT_SET, cv.UNDEFINED),
    ("vac_undervoltage", FAULT_STATUS, 7, 0x01, _BIT_SET, DEVICE_CLASS_PROBLEM),
    ("vac_overvoltage", FAULT_STATUS, 6, 0x01, _BIT_SET, DEVICE_CLASS_PROBLEM),
    ("ibat_overcurrent", FAULT_STATUS, 5, 0x01, _BIT_SET, DEVICE_CLASS_PROBLEM),
    ("vbat_overvoltage", FAULT_STATUS, 4, 0x01, _BIT_SET, DEVICE_CLASS_PROBLEM),
    ("thermal_shutdown", FAULT_STATUS, 3, 0x01, _BIT_SET, DEVICE_CLASS_PROBLEM),
    ("charge_safety_timer", FAULT_STATUS, 2, 0x01, _BIT_SET, DEVICE_CLASS_PROBLEM),
    ("drv_supply_invalid", FAULT_STATUS, 1, 0x01, _BIT_SET, DEVICE_CLASS_PROBLEM),
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
            cv.Optional(key): binary_sensor.binary_sensor_schema(
                device_class=device_class
            )
            for key, *_, device_class in _FIELDS
        }
    ),
    cv.has_at_least_one_key(*(key for key, *_ in _FIELDS)),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25820_ID])
    sensors = binary_sensor.sub_binary_sensors(config)
    for key, reg, shift, mask, match, _ in _FIELDS:
        if (var := await sensors(key, None)) is not None:
            state = _state_expression(reg, shift, mask, match)
            STATUS_PUBLISHER.add(hub, parent_ref(var).publish_state(state))
            DUMP_STATUS.add(hub, bq25820_ns.dump_status_entity(parent_ref(var)))
