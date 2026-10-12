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

from . import BQ25186_LISTENER_SCHEMA, CONF_BQ25186_ID, bq25186_ns, register_listener

DEPENDENCIES = ["bq25186"]

BQ25186BinarySensor = bq25186_ns.class_(
    "BQ25186BinarySensor", binary_sensor.BinarySensor
)

REG_STAT0 = 0
REG_STAT1 = 1
REG_FLAG0 = 2
# Bit set for each field value that reads as on: a single bit is on when it is 1
_BIT_SET = 0b10

# (key, status register, bit shift, field mask, on-values bitmap, device class)
_FIELDS = (
    ("ts", REG_STAT0, 7, 0x01, _BIT_SET, DEVICE_CLASS_OPENING),
    ("charging", REG_STAT0, 5, 0x03, 0b0110, DEVICE_CLASS_BATTERY_CHARGING),
    ("charge_done", REG_STAT0, 5, 0x03, 0b1000, None),
    ("ilim", REG_STAT0, 4, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("vdppm", REG_STAT0, 3, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("vindpm", REG_STAT0, 2, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("thermreg", REG_STAT0, 1, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("power_good", REG_STAT0, 0, 0x01, _BIT_SET, DEVICE_CLASS_PLUG),
    ("vin_ovp", REG_STAT1, 7, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("battery_uvlo", REG_STAT1, 6, 0x01, _BIT_SET, DEVICE_CLASS_RUNNING),
    ("safety_timer_flag", REG_STAT1, 2, 0x01, _BIT_SET, None),
    ("wake1_flag", REG_STAT1, 1, 0x01, _BIT_SET, None),
    ("wake2_flag", REG_STAT1, 0, 0x01, _BIT_SET, None),
    ("ts_flag", REG_FLAG0, 7, 0x01, _BIT_SET, None),
    ("ilim_flag", REG_FLAG0, 6, 0x01, _BIT_SET, None),
    ("vdppm_flag", REG_FLAG0, 5, 0x01, _BIT_SET, None),
    ("vindpm_flag", REG_FLAG0, 4, 0x01, _BIT_SET, None),
    ("thermreg_flag", REG_FLAG0, 3, 0x01, _BIT_SET, None),
    ("vin_ovp_flag", REG_FLAG0, 2, 0x01, _BIT_SET, None),
    ("battery_uvlo_flag", REG_FLAG0, 1, 0x01, _BIT_SET, None),
    ("battery_ocp_flag", REG_FLAG0, 0, 0x01, _BIT_SET, None),
)


def _schema(device_class: str | None) -> cv.Schema:
    if device_class is None:
        return binary_sensor.binary_sensor_schema(BQ25186BinarySensor)
    return binary_sensor.binary_sensor_schema(
        BQ25186BinarySensor, device_class=device_class
    )


CONFIG_SCHEMA = cv.All(
    BQ25186_LISTENER_SCHEMA.extend(
        {cv.Optional(key): _schema(device_class) for key, *_, device_class in _FIELDS}
    ),
    cv.has_at_least_one_key(*(key for key, *_ in _FIELDS)),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BQ25186_ID])
    for key, reg, shift, mask, match, _ in _FIELDS:
        if (conf := config.get(key)) is not None:
            var = await binary_sensor.new_binary_sensor(conf, reg, shift, mask, match)
            await register_listener(hub, var)
