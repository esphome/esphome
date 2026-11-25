import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import (
    CONF_BLUE_CHANNEL,
    CONF_CLEAR_CHANNEL,
    CONF_GREEN_CHANNEL,
    CONF_RED_CHANNEL,
    UNIT_COUNTS,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLOR_TEMPERATURE,
    CONF_ILLUMINANCE,
    ICON_LIGHTBULB,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_KELVIN,
)
from esphome.types import ConfigType

from . import CONF_BH1745_ID, BH1745Component

DEPENDENCIES = ["bh1745"]

color_channel_schema = sensor.sensor_schema(
    unit_of_measurement=UNIT_COUNTS,
    icon=ICON_LIGHTBULB,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_BH1745_ID): cv.use_id(BH1745Component),
        cv.Optional(CONF_RED_CHANNEL): color_channel_schema,
        cv.Optional(CONF_GREEN_CHANNEL): color_channel_schema,
        cv.Optional(CONF_BLUE_CHANNEL): color_channel_schema,
        cv.Optional(CONF_CLEAR_CHANNEL): color_channel_schema,
        cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
            kind=sensor.ILLUMINANCE, icon=ICON_LIGHTBULB, accuracy_decimals=1
        ),
        cv.Optional(CONF_COLOR_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_KELVIN,
            icon=ICON_THERMOMETER,
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_BH1745_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RED_CHANNEL, hub.set_red_counts_sensor)
    await sensors(CONF_GREEN_CHANNEL, hub.set_green_counts_sensor)
    await sensors(CONF_BLUE_CHANNEL, hub.set_blue_counts_sensor)
    await sensors(CONF_CLEAR_CHANNEL, hub.set_clear_counts_sensor)
    await sensors(CONF_ILLUMINANCE, hub.set_illuminance_sensor)
    await sensors(CONF_COLOR_TEMPERATURE, hub.set_color_temperature_sensor)
