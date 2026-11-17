import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import (
    CONF_BLUE_CHANNEL,
    CONF_CLEAR_CHANNEL,
    CONF_GREEN_CHANNEL,
    CONF_RED_CHANNEL,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_TEMPERATURE,
    CONF_GAIN,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_ILLUMINANCE,
    CONF_INTEGRATION_TIME,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_LIGHTBULB,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_KELVIN,
    UNIT_LUX,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

DEPENDENCIES = ["i2c"]

tcs34725_ns = cg.esphome_ns.namespace("tcs34725")
TCS34725Component = tcs34725_ns.class_(
    "TCS34725Component", cg.PollingComponent, i2c.I2CDevice
)

TCS34725IntegrationTime = tcs34725_ns.enum("TCS34725IntegrationTime")
TCS34725_INTEGRATION_TIMES = TCS34725IntegrationTime.options(
    "auto",
    "2.4ms",
    "24ms",
    "50ms",
    "101ms",
    "120ms",
    "154ms",
    "180ms",
    "199ms",
    "240ms",
    "300ms",
    "360ms",
    "401ms",
    "420ms",
    "480ms",
    "499ms",
    "540ms",
    "600ms",
    "614ms",
    prefix="TCS34725_INTEGRATION_TIME_",
)

TCS34725Gain = tcs34725_ns.enum("TCS34725Gain")
TCS34725_GAINS = TCS34725Gain.options("1X", "4X", "16X", "60X", prefix="TCS34725_GAIN_")

color_channel_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    icon=ICON_LIGHTBULB,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)
color_temperature_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_KELVIN,
    icon=ICON_THERMOMETER,
    accuracy_decimals=1,
    state_class=STATE_CLASS_MEASUREMENT,
)
illuminance_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_LUX,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_ILLUMINANCE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TCS34725Component),
            cv.Optional(CONF_RED_CHANNEL): color_channel_schema,
            cv.Optional(CONF_GREEN_CHANNEL): color_channel_schema,
            cv.Optional(CONF_BLUE_CHANNEL): color_channel_schema,
            cv.Optional(CONF_CLEAR_CHANNEL): color_channel_schema,
            cv.Optional(CONF_ILLUMINANCE): illuminance_schema,
            cv.Optional(CONF_COLOR_TEMPERATURE): color_temperature_schema,
            cv.Optional(CONF_INTEGRATION_TIME, default="auto"): cv.enum(
                TCS34725_INTEGRATION_TIMES, lower=True
            ),
            cv.Optional(CONF_GAIN, default="1X"): cv.enum(TCS34725_GAINS, upper=True),
            cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.float_range(
                min=1.0
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x29))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add(var.set_integration_time(config[CONF_INTEGRATION_TIME]))
    cg.add(var.set_gain(config[CONF_GAIN]))
    cg.add(var.set_glass_attenuation_factor(config[CONF_GLASS_ATTENUATION_FACTOR]))

    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RED_CHANNEL, var.set_red_sensor)
    await sensors(CONF_GREEN_CHANNEL, var.set_green_sensor)
    await sensors(CONF_BLUE_CHANNEL, var.set_blue_sensor)
    await sensors(CONF_CLEAR_CHANNEL, var.set_clear_sensor)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
    await sensors(CONF_COLOR_TEMPERATURE, var.set_color_temperature_sensor)
