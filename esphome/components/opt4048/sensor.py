from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c, sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_TEMPERATURE,
    CONF_ID,
    CONF_ILLUMINANCE,
    CONF_INTERRUPT_PIN,
    CONF_MODE,
    CONF_RANGE,
    CONF_X,
    CONF_Y,
    DEVICE_CLASS_EMPTY,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_BRIGHTNESS_5,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_EMPTY,
    UNIT_KELVIN,
    UNIT_LUX,
)
from esphome.types import ConfigType

CODEOWNERS = ["@JS3910"]
DEPENDENCIES = ["i2c"]

CONF_CHANNEL_X = "channel_x"
CONF_CHANNEL_Y = "channel_y"
CONF_CHANNEL_Z = "channel_z"
CONF_CHANNEL_W = "channel_w"
CONF_CONVERSION_TIME = "conversion_time"
CONF_QUICK_WAKE = "quick_wake"
CONF_FAULT_COUNT = "fault_count"
CONF_THRESHOLD_LOW = "threshold_low"
CONF_THRESHOLD_HIGH = "threshold_high"
CONF_THRESHOLD_CHANNEL = "threshold_channel"
CONF_INTERRUPT_LATCH = "interrupt_latch"
CONF_INTERRUPT_POLARITY = "interrupt_polarity"
CONF_INTERRUPT_DIRECTION = "interrupt_direction"
CONF_INTERRUPT_CONFIG = "interrupt_config"

opt4048_ns = cg.esphome_ns.namespace("opt4048")
OPT4048Component = opt4048_ns.class_(
    "OPT4048Component", cg.PollingComponent, i2c.I2CDevice
)

OPT4048Range = opt4048_ns.enum("OPT4048Range", is_class=True)
RANGES = {
    "2.2klux": OPT4048Range.OPT4048_RANGE_2K2,
    "4.5klux": OPT4048Range.OPT4048_RANGE_4K5,
    "9klux": OPT4048Range.OPT4048_RANGE_9K,
    "18klux": OPT4048Range.OPT4048_RANGE_18K,
    "36klux": OPT4048Range.OPT4048_RANGE_36K,
    "72klux": OPT4048Range.OPT4048_RANGE_72K,
    "144klux": OPT4048Range.OPT4048_RANGE_144K,
    "auto": OPT4048Range.OPT4048_RANGE_AUTO,
}

OPT4048ConversionTime = opt4048_ns.enum("OPT4048ConversionTime", is_class=True)
CONVERSION_TIMES = {
    "600us": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_600US,
    "1ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_1MS,
    "1.8ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_1_8MS,
    "3.4ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_3_4MS,
    "6.5ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_6_5MS,
    "12.7ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_12_7MS,
    "25ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_25MS,
    "50ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_50MS,
    "100ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_100MS,
    "200ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_200MS,
    "400ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_400MS,
    "800ms": OPT4048ConversionTime.OPT4048_CONVERSION_TIME_800MS,
}

OPT4048Mode = opt4048_ns.enum("OPT4048Mode", is_class=True)
MODES = {
    "powerdown": OPT4048Mode.OPT4048_MODE_POWERDOWN,
    "auto_oneshot": OPT4048Mode.OPT4048_MODE_AUTO_ONESHOT,
    "oneshot": OPT4048Mode.OPT4048_MODE_ONESHOT,
    "continuous": OPT4048Mode.OPT4048_MODE_CONTINUOUS,
}

OPT4048FaultCount = opt4048_ns.enum("OPT4048FaultCount", is_class=True)
FAULT_COUNTS = {
    1: OPT4048FaultCount.OPT4048_FAULT_COUNT_1,
    2: OPT4048FaultCount.OPT4048_FAULT_COUNT_2,
    4: OPT4048FaultCount.OPT4048_FAULT_COUNT_4,
    8: OPT4048FaultCount.OPT4048_FAULT_COUNT_8,
}

OPT4048IntConfig = opt4048_ns.enum("OPT4048IntConfig", is_class=True)
INT_CONFIGS = {
    "smbus_alert": OPT4048IntConfig.OPT4048_INT_CONFIG_SMBUS_ALERT,
    "data_ready_next": OPT4048IntConfig.OPT4048_INT_CONFIG_DATA_READY_NEXT,
    "data_ready_all": OPT4048IntConfig.OPT4048_INT_CONFIG_DATA_READY_ALL,
}

POLARITIES = {
    "high": True,
    "low": False,
}

DIRECTIONS = {
    "high": True,
    "low": False,
}

UNIT_COUNTS = "#"

ILLUMINANCE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_LUX,
    accuracy_decimals=2,
    device_class=DEVICE_CLASS_ILLUMINANCE,
    state_class=STATE_CLASS_MEASUREMENT,
)
CHROMATICITY_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_EMPTY,
    accuracy_decimals=4,
    device_class=DEVICE_CLASS_EMPTY,
    state_class=STATE_CLASS_MEASUREMENT,
)
COLOR_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_KELVIN,
    icon=ICON_THERMOMETER,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)
CHANNEL_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_COUNTS,
    icon=ICON_BRIGHTNESS_5,
    accuracy_decimals=0,
    device_class=DEVICE_CLASS_EMPTY,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OPT4048Component),
            cv.Optional(CONF_ILLUMINANCE): ILLUMINANCE_SCHEMA,
            cv.Optional(CONF_X): CHROMATICITY_SCHEMA,
            cv.Optional(CONF_Y): CHROMATICITY_SCHEMA,
            cv.Optional(CONF_COLOR_TEMPERATURE): COLOR_TEMPERATURE_SCHEMA,
            cv.Optional(CONF_CHANNEL_X): CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_Y): CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_Z): CHANNEL_SCHEMA,
            cv.Optional(CONF_CHANNEL_W): CHANNEL_SCHEMA,
            cv.Optional(CONF_RANGE, default="auto"): cv.enum(RANGES, lower=True),
            cv.Optional(CONF_CONVERSION_TIME, default="100ms"): cv.enum(
                CONVERSION_TIMES, lower=True
            ),
            cv.Optional(CONF_MODE, default="oneshot"): cv.enum(MODES, lower=True),
            cv.Optional(CONF_QUICK_WAKE, default=False): cv.boolean,
            cv.Optional(CONF_FAULT_COUNT, default=1): cv.enum(FAULT_COUNTS, int=True),
            cv.Optional(CONF_THRESHOLD_LOW): cv.uint32_t,
            cv.Optional(CONF_THRESHOLD_HIGH): cv.uint32_t,
            cv.Optional(CONF_THRESHOLD_CHANNEL, default="y"): cv.enum(
                {"x": 0, "y": 1, "z": 2, "w": 3}, lower=True
            ),
            cv.Optional(CONF_INTERRUPT_LATCH, default=True): cv.boolean,
            cv.Optional(CONF_INTERRUPT_POLARITY, default="high"): cv.enum(
                POLARITIES, lower=True
            ),
            cv.Optional(CONF_INTERRUPT_DIRECTION, default="high"): cv.enum(
                DIRECTIONS, lower=True
            ),
            cv.Optional(CONF_INTERRUPT_CONFIG, default="data_ready_all"): cv.enum(
                INT_CONFIGS, lower=True
            ),
            cv.Optional(CONF_INTERRUPT_PIN): pins.gpio_input_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x44))
)

SENSORS = {
    CONF_ILLUMINANCE: "set_illuminance_sensor",
    CONF_X: "set_x_sensor",
    CONF_Y: "set_y_sensor",
    CONF_COLOR_TEMPERATURE: "set_color_temperature_sensor",
    CONF_CHANNEL_X: "set_channel_x_sensor",
    CONF_CHANNEL_Y: "set_channel_y_sensor",
    CONF_CHANNEL_Z: "set_channel_z_sensor",
    CONF_CHANNEL_W: "set_channel_w_sensor",
}


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    cg.add(var.set_range(config[CONF_RANGE]))
    cg.add(var.set_conversion_time(config[CONF_CONVERSION_TIME]))
    cg.add(var.set_mode(config[CONF_MODE]))
    cg.add(var.set_quick_wake(config[CONF_QUICK_WAKE]))
    cg.add(var.set_fault_count(config[CONF_FAULT_COUNT]))
    cg.add(var.set_threshold_channel(config[CONF_THRESHOLD_CHANNEL]))
    cg.add(var.set_interrupt_latch(config[CONF_INTERRUPT_LATCH]))
    cg.add(var.set_interrupt_polarity(config[CONF_INTERRUPT_POLARITY]))
    cg.add(var.set_interrupt_direction(config[CONF_INTERRUPT_DIRECTION]))
    cg.add(var.set_interrupt_config(config[CONF_INTERRUPT_CONFIG]))

    if (threshold_low := config.get(CONF_THRESHOLD_LOW)) is not None:
        cg.add(var.set_threshold_low(threshold_low))
    if (threshold_high := config.get(CONF_THRESHOLD_HIGH)) is not None:
        cg.add(var.set_threshold_high(threshold_high))
    if interrupt_pin := config.get(CONF_INTERRUPT_PIN):
        pin = await cg.gpio_pin_expression(interrupt_pin)
        cg.add(var.set_interrupt_pin(pin))

    for conf_id, setter in SENSORS.items():
        if sensor_config := config.get(conf_id):
            sens = await sensor.new_sensor(sensor_config)
            cg.add(getattr(var, setter)(sens))
