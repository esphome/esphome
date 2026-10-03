import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.const import CONF_ACCELEROMETER_ODR, CONF_ACCELEROMETER_RANGE
from esphome.components.motion import motion_schema, new_motion_component
import esphome.config_validation as cv
from esphome.const import CONF_DURATION, CONF_INTERRUPT, CONF_PIN, CONF_THRESHOLD
from esphome.types import ConfigType

from . import LIS3DHComponent, lis3dh_ns

CONF_OPERATING_MODE = "operating_mode"
CONF_AXES = "axes"
CONF_LATCHED = "latched"
CONF_ACTIVE_HIGH = "active_high"
CONF_HIGH_PASS_FILTER = "high_pass_filter"

#  Enum proxies (must match the C++ enum values exactly)
LIS3DHRange = lis3dh_ns.enum("LIS3DHRange")
RANGE_OPTIONS = {
    "2G": LIS3DHRange.LIS3DH_RANGE_2G,
    "4G": LIS3DHRange.LIS3DH_RANGE_4G,
    "8G": LIS3DHRange.LIS3DH_RANGE_8G,
    "16G": LIS3DHRange.LIS3DH_RANGE_16G,
}

LIS3DHDataRate = lis3dh_ns.enum("LIS3DHDataRate")
DATA_RATE_OPTIONS = {
    "1HZ": LIS3DHDataRate.LIS3DH_DATA_RATE_1HZ,
    "10HZ": LIS3DHDataRate.LIS3DH_DATA_RATE_10HZ,
    "25HZ": LIS3DHDataRate.LIS3DH_DATA_RATE_25HZ,
    "50HZ": LIS3DHDataRate.LIS3DH_DATA_RATE_50HZ,
    "100HZ": LIS3DHDataRate.LIS3DH_DATA_RATE_100HZ,
    "200HZ": LIS3DHDataRate.LIS3DH_DATA_RATE_200HZ,
    "400HZ": LIS3DHDataRate.LIS3DH_DATA_RATE_400HZ,
    # 1620HZ is only available in low-power mode.
    "1620HZ": LIS3DHDataRate.LIS3DH_DATA_RATE_1620HZ,
    # 1344HZ in normal / high-resolution mode; 5376HZ in low-power mode.
    "1344HZ": LIS3DHDataRate.LIS3DH_DATA_RATE_1344HZ,
}

LIS3DHOperatingMode = lis3dh_ns.enum("LIS3DHOperatingMode")
OPERATING_MODE_OPTIONS = {
    "LOW_POWER": LIS3DHOperatingMode.LIS3DH_MODE_LOW_POWER,
    "NORMAL": LIS3DHOperatingMode.LIS3DH_MODE_NORMAL,
    "HIGH_RESOLUTION": LIS3DHOperatingMode.LIS3DH_MODE_HIGH_RESOLUTION,
}

LIS3DHInterruptPin = lis3dh_ns.enum("LIS3DHInterruptPin")
INTERRUPT_PIN_OPTIONS = {
    "INT1": LIS3DHInterruptPin.LIS3DH_INT_PIN_INT1,
    "INT2": LIS3DHInterruptPin.LIS3DH_INT_PIN_INT2,
}

AXES = ["x", "y", "z"]


def _axes(value):
    """A list of one or more of the axes 'x', 'y' and 'z'."""
    value = cv.ensure_list(cv.one_of(*AXES, lower=True))(value)
    if not value:
        raise cv.Invalid("Specify at least one axis")
    if len(set(value)) != len(value):
        raise cv.Invalid("Each axis may be listed only once")
    return value


# The motion (activity) interrupt keeps the physical INT pad asserted while the
# device is moving. Wire the pad to a GPIO and use it as the `wakeup_pin` of the
# `deep_sleep` component to wake an ESP32 from deep sleep on motion.
#
# By default the threshold is compared against the total measured acceleration,
# which always includes the ~1 g of gravity, so it must be set above 1 g to avoid
# triggering at rest. Enable `high_pass_filter` to remove gravity from the
# comparison, which lets you use a small threshold (the change in acceleration).
INTERRUPT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PIN, default="INT1"): cv.enum(
            INTERRUPT_PIN_OPTIONS, upper=True
        ),
        cv.Optional(CONF_THRESHOLD, default="1.1g"): cv.All(
            cv.float_with_unit("acceleration", "(g|G)?"), cv.positive_float
        ),
        cv.Optional(CONF_DURATION, default=0): cv.int_range(min=0, max=127),
        cv.Optional(CONF_AXES, default=AXES): _axes,
        cv.Optional(CONF_LATCHED, default=True): cv.boolean,
        cv.Optional(CONF_ACTIVE_HIGH, default=True): cv.boolean,
        cv.Optional(CONF_HIGH_PASS_FILTER, default=False): cv.boolean,
    }
)


def _validate_odr_mode(config: ConfigType) -> ConfigType:
    # Two output data rates are tied to the operating mode (they share a register
    # code whose meaning depends on the mode): 1620Hz exists only in low-power
    # mode, and code 1344Hz becomes 5376Hz in low-power mode.
    # A validated enum is the option's name, not the value it maps to, so these
    # are compared as the names.
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


#  Top-level CONFIG_SCHEMA
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
)


#  Code generation
async def to_code(config):
    var = await new_motion_component(config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_range(config[CONF_ACCELEROMETER_RANGE]))
    cg.add(var.set_data_rate(config[CONF_ACCELEROMETER_ODR]))
    cg.add(var.set_operating_mode(config[CONF_OPERATING_MODE]))

    if (interrupt := config.get(CONF_INTERRUPT)) is not None:
        axes = interrupt[CONF_AXES]
        cg.add(
            var.set_interrupt(
                interrupt[CONF_PIN],
                interrupt[CONF_THRESHOLD],
                interrupt[CONF_DURATION],
                "x" in axes,
                "y" in axes,
                "z" in axes,
                interrupt[CONF_LATCHED],
                interrupt[CONF_ACTIVE_HIGH],
                interrupt[CONF_HIGH_PASS_FILTER],
            )
        )
