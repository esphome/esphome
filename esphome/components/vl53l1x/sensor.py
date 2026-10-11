from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ADDRESS,
    CONF_ENABLE_PIN,
    CONF_OFFSET,
    ICON_ARROW_EXPAND_VERTICAL,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
)
from esphome.types import ConfigType

CODEOWNERS = ["@will-tm"]
DEPENDENCIES = ["i2c"]

vl53l1x_ns = cg.esphome_ns.namespace("vl53l1x")
VL53L1XSensor = vl53l1x_ns.class_(
    "VL53L1XSensor", sensor.Sensor, cg.PollingComponent, i2c.I2CDevice
)

DistanceMode = vl53l1x_ns.enum("DistanceMode")
DISTANCE_MODE_OPTIONS = {
    "short": DistanceMode.DISTANCE_MODE_SHORT,
    "long": DistanceMode.DISTANCE_MODE_LONG,
}

CONF_DISTANCE_MODE = "distance_mode"
CONF_TIMING_BUDGET = "timing_budget"
CONF_SIGNAL_RATE_LIMIT = "signal_rate_limit"
CONF_ROI_WIDTH = "roi_width"
CONF_ROI_HEIGHT = "roi_height"
CONF_ROI_CENTER = "roi_center"

DEFAULT_ADDRESS = 0x29
DEFAULT_ROI_CENTER = 199
# Timing budgets in ms supported by the ST Ultra Lite Driver tables, per distance mode
TIMING_BUDGETS = {
    "short": (15, 20, 33, 50, 100, 200, 500),
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
    if (config[CONF_ROI_WIDTH] > 10 or config[CONF_ROI_HEIGHT] > 10) and config[
        CONF_ROI_CENTER
    ] != DEFAULT_ROI_CENTER:
        raise cv.Invalid(
            f"roi_center must be {DEFAULT_ROI_CENTER} when roi_width or roi_height "
            "is larger than 10",
            path=[CONF_ROI_CENTER],
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
            cv.Optional(CONF_DISTANCE_MODE, default="long"): cv.enum(
                DISTANCE_MODE_OPTIONS, lower=True
            ),
            cv.Optional(
                CONF_TIMING_BUDGET, default="50ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
            # Minimum return signal rate in MCPS (mega counts per second)
            cv.Optional(CONF_SIGNAL_RATE_LIMIT, default=1.0): cv.float_range(
                min=0.0, max=512.0, min_included=False, max_included=False
            ),
            cv.Optional(CONF_ROI_WIDTH, default=16): cv.int_range(min=4, max=16),
            cv.Optional(CONF_ROI_HEIGHT, default=16): cv.int_range(min=4, max=16),
            cv.Optional(CONF_ROI_CENTER, default=DEFAULT_ROI_CENTER): cv.int_range(
                min=0, max=255
            ),
            cv.Optional(CONF_OFFSET, default=0): cv.int_range(min=-1024, max=1023),
        }
    )
    .extend(cv.polling_component_schema("1s"))
    .extend(i2c.i2c_device_schema(DEFAULT_ADDRESS)),
    validate_address,
    validate_timing_budget,
    validate_roi,
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_distance_mode(config[CONF_DISTANCE_MODE]))
    cg.add(var.set_timing_budget(config[CONF_TIMING_BUDGET].total_milliseconds))
    cg.add(var.set_signal_rate_limit(config[CONF_SIGNAL_RATE_LIMIT]))
    cg.add(
        var.set_roi(
            config[CONF_ROI_WIDTH], config[CONF_ROI_HEIGHT], config[CONF_ROI_CENTER]
        )
    )
    cg.add(var.set_offset(config[CONF_OFFSET]))

    if (enable_pin := config.get(CONF_ENABLE_PIN)) is not None:
        cg.add(var.set_enable_pin(await cg.gpio_pin_expression(enable_pin)))
