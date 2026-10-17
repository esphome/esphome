import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_HUMIDITY,
    CONF_ILLUMINANCE,
    CONF_TEMPERATURE,
    CONF_WIND_DIRECTION_DEGREES,
    CONF_WIND_SPEED,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_ILLUMINANCE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_WIND_DIRECTION,
    DEVICE_CLASS_WIND_SPEED,
    ICON_COUNTER,
    ICON_GRAIN,
    ICON_SIGN_DIRECTION,
    ICON_WEATHER_WINDY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_CELSIUS,
    UNIT_DEGREES,
    UNIT_LUX,
    UNIT_METER_PER_SECOND,
    UNIT_MILLIMETER,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from . import CONF_VEVOR_DTX_ID, VevorDtxComponent

DEPENDENCIES = ["vevor_dtx"]

CONF_RAIN = "rain"
CONF_TX_COUNTER = "tx_counter"
CONF_UV_INDEX = "uv_index"
CONF_WIND_GUST = "wind_gust"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_VEVOR_DTX_ID): cv.use_id(VevorDtxComponent),
        cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_WIND_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER_PER_SECOND,
            icon=ICON_WEATHER_WINDY,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_WIND_SPEED,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_WIND_GUST): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER_PER_SECOND,
            icon=ICON_WEATHER_WINDY,
            accuracy_decimals=1,
            device_class=DEVICE_CLASS_WIND_SPEED,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_WIND_DIRECTION_DEGREES): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            icon=ICON_SIGN_DIRECTION,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_WIND_DIRECTION,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_RAIN): sensor.sensor_schema(
            unit_of_measurement=UNIT_MILLIMETER,
            icon=ICON_GRAIN,
            accuracy_decimals=1,
            state_class=STATE_CLASS_TOTAL_INCREASING,
        ),
        cv.Optional(CONF_UV_INDEX): sensor.sensor_schema(
            accuracy_decimals=0,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
            unit_of_measurement=UNIT_LUX,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_ILLUMINANCE,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_TX_COUNTER): sensor.sensor_schema(
            accuracy_decimals=0,
            icon=ICON_COUNTER,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_VEVOR_DTX_ID])
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_TEMPERATURE, hub.set_temperature_sensor)
    await sensors(CONF_HUMIDITY, hub.set_humidity_sensor)
    await sensors(CONF_WIND_SPEED, hub.set_wind_speed_sensor)
    await sensors(CONF_WIND_GUST, hub.set_wind_gust_sensor)
    await sensors(CONF_WIND_DIRECTION_DEGREES, hub.set_wind_direction_sensor)
    await sensors(CONF_RAIN, hub.set_rain_sensor)
    await sensors(CONF_UV_INDEX, hub.set_uv_index_sensor)
    await sensors(CONF_ILLUMINANCE, hub.set_illuminance_sensor)
    await sensors(CONF_TX_COUNTER, hub.set_tx_counter_sensor)
