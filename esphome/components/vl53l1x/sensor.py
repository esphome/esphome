import struct

from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.i2c import CONF_I2C_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ENABLE_PIN,
    CONF_OFFSET,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import DOMAIN

CODEOWNERS = ["@will-tm"]
DEPENDENCIES = ["i2c"]

vl53l1x_ns = cg.esphome_ns.namespace("vl53l1x")
VL53L1XSensor = vl53l1x_ns.class_(
    "VL53L1XSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

DistanceMode = vl53l1x_ns.enum("DistanceMode")
DISTANCE_MODE_OPTIONS = DistanceMode.options(
    "short", "medium", "long", prefix="DISTANCE_MODE_"
)

CONF_DISTANCE_MODE = "distance_mode"
CONF_TIMING_BUDGET = "timing_budget"
CONF_SIGNAL_RATE_LIMIT = "signal_rate_limit"
CONF_ROI_WIDTH = "roi_width"
CONF_ROI_HEIGHT = "roi_height"
CONF_ROI_CENTER = "roi_center"

DEFAULT_ADDRESS = 0x29
# The signal rate limit register holds MCPS as 9.7 fixed point
SIGNAL_RATE_SCALE = 128
# Keep in sync with the initializers in vl53l1x.h
DEFAULT_DISTANCE_MODE = "long"
DEFAULT_TIMING_BUDGET_MS = 50
DEFAULT_SIGNAL_RATE_LIMIT = 1.0
DEFAULT_ROI_SIZE = 16
DEFAULT_OFFSET = 0
# ROI centre the ULD uses for a ROI wider or taller than 10 SPADs
ROI_CENTER_LARGE = 199
# Timing budgets in ms supported by the ST Ultra Lite Driver tables, per distance mode
TIMING_BUDGETS = {
    "short": (15, 20, 33, 50, 100, 200, 500),
    "medium": (20, 33, 50, 100, 200, 500),
    "long": (20, 33, 50, 100, 200, 500),
}


def validate_address(config: ConfigType) -> ConfigType:
    if config[CONF_ADDRESS] != DEFAULT_ADDRESS and CONF_ENABLE_PIN not in config:
        raise cv.Invalid(
            "Address other than 0x29 requires enable_pin definition to allow sensor "
            "re-addressing. If you have multiple VL53L1X sensors on the same I2C bus, "
            "each sensor must have an enable_pin defined."
        )
    return config


def validate_timing_budget(config: ConfigType) -> ConfigType:
    mode = config[CONF_DISTANCE_MODE]
    budgets = TIMING_BUDGETS[mode]
    if config[CONF_TIMING_BUDGET].total_milliseconds not in budgets:
        raise cv.Invalid(
            f"Timing budget for {mode} distance mode must be one of "
            f"{', '.join(f'{b}ms' for b in budgets)}",
            path=[CONF_TIMING_BUDGET],
        )
    return config


def validate_roi(config: ConfigType) -> ConfigType:
    if (
        (center := config.get(CONF_ROI_CENTER)) is not None
        and center != ROI_CENTER_LARGE
        and (config[CONF_ROI_WIDTH] > 10 or config[CONF_ROI_HEIGHT] > 10)
    ):
        raise cv.Invalid(
            f"roi_center must be {ROI_CENTER_LARGE} when roi_width or roi_height "
            "is larger than 10",
            path=[CONF_ROI_CENTER],
        )
    return config


def _final_validate(config: ConfigType) -> ConfigType:
    # Every sensor boots at 0x29. With several on one bus each needs its own enable_pin so it
    # can be held in reset while the others are re-addressed, and none may stay at 0x29, or a
    # later sibling boots at the same address and the re-address write reaches both chips.
    siblings = fv.platform_entries(
        domain=sensor.DOMAIN, platform=DOMAIN, match={CONF_I2C_ID: config[CONF_I2C_ID]}
    )
    if len(siblings) < 2:
        return config
    if CONF_ENABLE_PIN not in config:
        raise cv.Invalid(
            "Every VL53L1X sensor on a shared I2C bus needs an enable_pin",
            path=[CONF_ENABLE_PIN],
        )
    address = config[CONF_ADDRESS]
    if address == DEFAULT_ADDRESS:
        raise cv.Invalid(
            f"VL53L1X sensors sharing an I2C bus must all be re-addressed away from "
            f"0x{DEFAULT_ADDRESS:02X}",
            path=[CONF_ADDRESS],
        )
    if sum(conf[CONF_ADDRESS] == address for conf in siblings) > 1:
        raise cv.Invalid(
            f"Address 0x{address:02X} is used by more than one VL53L1X sensor on this bus",
            path=[CONF_ADDRESS],
        )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        VL53L1XSensor,
        unit_of_measurement=UNIT_METER,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
        icon=ICON_ARROW_EXPAND_VERTICAL,
    )
    .extend(
        {
            cv.Optional(CONF_DISTANCE_MODE, default=DEFAULT_DISTANCE_MODE): cv.enum(
                DISTANCE_MODE_OPTIONS, lower=True
            ),
            cv.Optional(
                CONF_TIMING_BUDGET, default=f"{DEFAULT_TIMING_BUDGET_MS}ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            # Minimum return signal rate in MCPS (mega counts per second)
            cv.Optional(
                CONF_SIGNAL_RATE_LIMIT, default=DEFAULT_SIGNAL_RATE_LIMIT
            ): cv.float_range(
                min=0.0, max=512.0, min_included=False, max_included=False
            ),
            cv.Optional(CONF_ROI_WIDTH, default=DEFAULT_ROI_SIZE): cv.int_range(
                min=4, max=16
            ),
            cv.Optional(CONF_ROI_HEIGHT, default=DEFAULT_ROI_SIZE): cv.int_range(
                min=4, max=16
            ),
            # Defaults to the chip's optical centre, or 199 for a ROI larger than 10x10
            cv.Optional(CONF_ROI_CENTER): cv.int_range(min=1, max=255),
            cv.Optional(CONF_OFFSET, default=DEFAULT_OFFSET): cv.int_range(
                min=-1024, max=1023
            ),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(DEFAULT_ADDRESS)),
    validate_address,
    validate_timing_budget,
    validate_roi,
)

FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)
    await sensor.register_sensor(var, config)

    # Skip the setters when the config matches the C++ initializers.
    if (mode := config[CONF_DISTANCE_MODE]) != DEFAULT_DISTANCE_MODE:
        cg.add(var.set_distance_mode(mode))
    budget_ms = config[CONF_TIMING_BUDGET].total_milliseconds
    if budget_ms != DEFAULT_TIMING_BUDGET_MS:
        cg.add(var.set_timing_budget(budget_ms))
    # Round to float32 first, as the old float setter did, so the register value is unchanged
    mcps = struct.unpack("<f", struct.pack("<f", config[CONF_SIGNAL_RATE_LIMIT]))[0]
    rate = int(mcps * SIGNAL_RATE_SCALE)
    if rate != DEFAULT_SIGNAL_RATE_LIMIT * SIGNAL_RATE_SCALE:
        cg.add(var.set_signal_rate_limit(rate))
    width, height = config[CONF_ROI_WIDTH], config[CONF_ROI_HEIGHT]
    if width != DEFAULT_ROI_SIZE or height != DEFAULT_ROI_SIZE:
        cg.add(var.set_roi(width, height))
    cg.add_optional(var.set_roi_center, config.get(CONF_ROI_CENTER))
    if (offset := config[CONF_OFFSET]) != DEFAULT_OFFSET:
        cg.add(var.set_offset(offset))

    await cg.add_pin(var.set_enable_pin, config.get(CONF_ENABLE_PIN))
