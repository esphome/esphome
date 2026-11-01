import logging
import math

import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.const import (
    CONF_ACCELEROMETER_ODR,
    CONF_ACCELEROMETER_RANGE,
    CONF_ACTIVE_HIGH,
)
from esphome.components.motion import AXES, motion_schema, new_motion_component
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_INTERRUPT, CONF_PIN, CONF_THRESHOLD
from esphome.types import ConfigType

from . import LIS3DHComponent, lis3dh_ns

_LOGGER = logging.getLogger(__name__)

CONF_OPERATING_MODE = "operating_mode"
CONF_AXES = "axes"
CONF_LATCHED = "latched"
CONF_HIGH_PASS_FILTER = "high_pass_filter"

# Enum values must match the C++ enums.
LIS3DHRange = lis3dh_ns.enum("LIS3DHRange")
RANGE_OPTIONS = LIS3DHRange.options("2G", "4G", "8G", "16G", prefix="LIS3DH_RANGE_")

LIS3DHDataRate = lis3dh_ns.enum("LIS3DHDataRate")
# 1620HZ is only available in low-power mode.
# 1344HZ in normal / high-resolution mode; 5376HZ in low-power mode.
DATA_RATE_OPTIONS = LIS3DHDataRate.options(
    "1HZ",
    "10HZ",
    "25HZ",
    "50HZ",
    "100HZ",
    "200HZ",
    "400HZ",
    "1620HZ",
    "1344HZ",
    prefix="LIS3DH_DATA_RATE_",
)

# Samples per second for each data rate key; INT1_DURATION counts samples.
ODR_HZ = {
    "1HZ": 1,
    "10HZ": 10,
    "25HZ": 25,
    "50HZ": 50,
    "100HZ": 100,
    "200HZ": 200,
    "400HZ": 400,
    "1620HZ": 1620,
    "1344HZ": 1344,
}
DURATION_MAX_REG = 0x7F

LIS3DHOperatingMode = lis3dh_ns.enum("LIS3DHOperatingMode")
OPERATING_MODE_OPTIONS = LIS3DHOperatingMode.options(
    "LOW_POWER", "NORMAL", "HIGH_RESOLUTION", prefix="LIS3DH_MODE_"
)

LIS3DHInterruptPin = lis3dh_ns.enum("LIS3DHInterruptPin")
INTERRUPT_PIN_OPTIONS = LIS3DHInterruptPin.options(
    "INT1", "INT2", prefix="LIS3DH_INT_PIN_"
)

# INT1_CFG high-event bit per axis.
INT_CFG_HIGH_EVENT = {"x": 0x02, "y": 0x08, "z": 0x20}

# Motion interrupt threshold LSB in mg, per range. INT1_THS holds seven bits.
THRESHOLD_LSB_MG = {"2G": 16, "4G": 32, "8G": 62, "16G": 186}
THRESHOLD_MAX_REG = 0x7F


def _threshold_register(threshold_g: float, range_key: str) -> int:
    """Return INT1_THS for a threshold in g: nearest step, at least 1 so the pad is not held asserted."""
    return max(math.floor(threshold_g * 1000 / THRESHOLD_LSB_MG[range_key] + 0.5), 1)


def _duration_register(duration_ms: int, odr_key: str) -> int:
    """Return INT1_DURATION for a time in ms: the nearest number of samples at the data rate."""
    return math.floor(duration_ms * ODR_HZ[odr_key] / 1000 + 0.5)


def _axes(value: object) -> list[str]:
    value = cv.All(cv.ensure_list(cv.one_of(*AXES, lower=True)), cv.Length(min=1))(
        value
    )
    if len(set(value)) != len(value):
        raise cv.Invalid("Each axis may be listed only once")
    return value


# Without high_pass_filter the threshold includes gravity, so it should exceed 1g.
INTERRUPT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PIN, default="INT1"): cv.enum(
            INTERRUPT_PIN_OPTIONS, upper=True
        ),
        cv.Optional(CONF_THRESHOLD, default="1.1g"): cv.All(
            cv.float_with_unit("acceleration", "(g|G)?"), cv.positive_float
        ),
        cv.Optional(CONF_DURATION, default="0ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_AXES, default=AXES): _axes,
        cv.Optional(CONF_LATCHED, default=True): cv.boolean,
        cv.Optional(CONF_ACTIVE_HIGH, default=True): cv.boolean,
        cv.Optional(CONF_HIGH_PASS_FILTER, default=False): cv.boolean,
    }
)


def _validate_odr_mode(config: ConfigType) -> ConfigType:
    # 1620Hz exists only in low power mode; the 1344Hz code runs at 5376Hz there.
    odr = config[CONF_ACCELEROMETER_ODR]
    low_power = config[CONF_OPERATING_MODE] == "LOW_POWER"
    if odr == "1620HZ" and not low_power:
        raise cv.Invalid(
            "'1620HZ' is only available with 'operating_mode: LOW_POWER'",
            path=[CONF_ACCELEROMETER_ODR],
        )
    if odr == "1344HZ" and low_power:
        raise cv.Invalid(
            "'1344HZ' is not available with 'operating_mode: LOW_POWER' "
            "(that combination runs at 5376Hz)",
            path=[CONF_ACCELEROMETER_ODR],
        )
    return config


def _validate_interrupt(config: ConfigType) -> ConfigType:
    if (interrupt := config.get(CONF_INTERRUPT)) is None:
        return config
    range_key = config[CONF_ACCELEROMETER_RANGE]
    threshold = interrupt[CONF_THRESHOLD]
    max_g = THRESHOLD_MAX_REG * THRESHOLD_LSB_MG[range_key] / 1000
    if threshold > max_g:
        raise cv.Invalid(
            f"'threshold' can be at most {max_g:g}g with 'accelerometer_range: {range_key}'",
            path=[CONF_INTERRUPT, CONF_THRESHOLD],
        )
    if not interrupt[CONF_HIGH_PASS_FILTER] and threshold <= 1.0:
        _LOGGER.warning(
            "lis3dh: a threshold of %sg or less includes gravity and may keep the interrupt "
            "asserted at rest; set 'high_pass_filter: true' to compare against motion only",
            threshold,
        )
    odr_key = config[CONF_ACCELEROMETER_ODR]
    duration_ms = interrupt[CONF_DURATION].total_milliseconds
    if _duration_register(duration_ms, odr_key) > DURATION_MAX_REG:
        max_ms = math.floor(DURATION_MAX_REG * 1000 / ODR_HZ[odr_key])
        raise cv.Invalid(
            f"'duration' can be at most {max_ms}ms with 'accelerometer_odr: {odr_key}'",
            path=[CONF_INTERRUPT, CONF_DURATION],
        )
    return config


CONFIG_SCHEMA = cv.All(
    motion_schema(LIS3DHComponent, has_accel=True, has_gyro=False)
    .extend(
        {
            cv.Optional(CONF_ACCELEROMETER_RANGE, default="2G"): cv.enum(
                RANGE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_ACCELEROMETER_ODR, default="100HZ"): cv.enum(
                DATA_RATE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_OPERATING_MODE, default="HIGH_RESOLUTION"): cv.enum(
                OPERATING_MODE_OPTIONS, upper=True
            ),
            cv.Optional(CONF_INTERRUPT): INTERRUPT_SCHEMA,
        }
    )
    .extend(i2c.i2c_device_schema(0x18)),
    _validate_odr_mode,
    _validate_interrupt,
)


async def to_code(config: ConfigType) -> None:
    var = await new_motion_component(config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_range(config[CONF_ACCELEROMETER_RANGE]))
    cg.add(var.set_data_rate(config[CONF_ACCELEROMETER_ODR]))
    cg.add(var.set_operating_mode(config[CONF_OPERATING_MODE]))

    if (interrupt := config.get(CONF_INTERRUPT)) is not None:
        threshold = _threshold_register(
            interrupt[CONF_THRESHOLD], config[CONF_ACCELEROMETER_RANGE]
        )
        axes_cfg = 0
        for axis in interrupt[CONF_AXES]:
            axes_cfg |= INT_CFG_HIGH_EVENT[axis]
        cg.add(
            var.set_interrupt(
                interrupt[CONF_PIN],
                threshold,
                _duration_register(
                    interrupt[CONF_DURATION].total_milliseconds,
                    config[CONF_ACCELEROMETER_ODR],
                ),
                axes_cfg,
                interrupt[CONF_LATCHED],
                interrupt[CONF_ACTIVE_HIGH],
                interrupt[CONF_HIGH_PASS_FILTER],
            )
        )
