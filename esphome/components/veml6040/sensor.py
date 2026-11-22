import esphome.codegen as cg
from esphome.components import i2c, sensor
from esphome.components.const import (
    CONF_BLUE_CHANNEL,
    CONF_GREEN_CHANNEL,
    CONF_RED_CHANNEL,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_TEMPERATURE,
    CONF_GLASS_ATTENUATION_FACTOR,
    CONF_ILLUMINANCE,
    CONF_INTEGRATION_TIME,
    ICON_LIGHTBULB,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_KELVIN,
)
from esphome.types import ConfigType

CODEOWNERS = ["@aalaei"]
DEPENDENCIES = ["i2c"]

CONF_COLOR_TEMPERATURE_OFFSET = "color_temperature_offset"
CONF_WHITE_CHANNEL = "white_channel"

veml6040_ns = cg.esphome_ns.namespace("veml6040")
VEML6040Component = veml6040_ns.class_(
    "VEML6040Component", cg.PollingComponent, i2c.I2CDevice
)

VEML6040IntegrationTime = veml6040_ns.enum("VEML6040IntegrationTime")
VEML6040_INTEGRATION_TIMES = VEML6040IntegrationTime.options(
    "40ms",
    "80ms",
    "160ms",
    "320ms",
    "640ms",
    "1280ms",
    prefix="VEML6040_INTEGRATION_TIME_",
)

color_channel_schema = sensor.sensor_schema(
    icon=ICON_LIGHTBULB,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(VEML6040Component),
            cv.Optional(CONF_RED_CHANNEL): color_channel_schema,
            cv.Optional(CONF_GREEN_CHANNEL): color_channel_schema,
            cv.Optional(CONF_BLUE_CHANNEL): color_channel_schema,
            cv.Optional(CONF_WHITE_CHANNEL): color_channel_schema,
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                kind=sensor.ILLUMINANCE, accuracy_decimals=1
            ),
            cv.Optional(CONF_COLOR_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_KELVIN,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_INTEGRATION_TIME, default="160ms"): cv.All(
                cv.positive_time_period_milliseconds,
                cv.enum(VEML6040_INTEGRATION_TIMES, lower=True),
            ),
            cv.Optional(CONF_GLASS_ATTENUATION_FACTOR, default=1.0): cv.float_range(
                min=1.0
            ),
            cv.Optional(CONF_COLOR_TEMPERATURE_OFFSET, default=0.5): cv.float_,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x10))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(
        config,
        config[CONF_INTEGRATION_TIME],
        config[CONF_GLASS_ATTENUATION_FACTOR],
        config[CONF_COLOR_TEMPERATURE_OFFSET],
    )
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RED_CHANNEL, var.set_red_sensor)
    await sensors(CONF_GREEN_CHANNEL, var.set_green_sensor)
    await sensors(CONF_BLUE_CHANNEL, var.set_blue_sensor)
    await sensors(CONF_WHITE_CHANNEL, var.set_white_sensor)
    await sensors(CONF_ILLUMINANCE, var.set_illuminance_sensor)
    await sensors(CONF_COLOR_TEMPERATURE, var.set_color_temperature_sensor)
