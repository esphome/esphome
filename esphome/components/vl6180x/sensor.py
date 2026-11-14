"""Sensor platform for VL6180X hubs."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_GAIN,
    CONF_RANGE,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ILLUMINANCE,
    ICON_BRIGHTNESS_5,
    ICON_RULER,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
    UNIT_MILLIMETER,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import (
    ALS_GAIN_FACTORS,
    ALS_GAINS,
    CONF_SCALER,
    CONF_VL6180X_ID,
    DOMAIN,
    VL6180XComponent,
)

DEPENDENCIES = ["vl6180x"]

CONF_LUX = "lux"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_VL6180X_ID): cv.use_id(VL6180XComponent),
            cv.Optional(CONF_RANGE): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIMETER,
                icon=ICON_RULER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DISTANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {cv.Optional(CONF_SCALER, default=1): cv.one_of(1, 2, 3, int=True)}
            ),
            cv.Optional(CONF_LUX): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                icon=ICON_BRIGHTNESS_5,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_GAIN, default=1): cv.enum(ALS_GAINS),
                    cv.Optional(
                        CONF_UPDATE_INTERVAL
                    ): cv.positive_time_period_milliseconds,
                }
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_RANGE, CONF_LUX),
)

FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=sensor.DOMAIN,
    platform=DOMAIN,
    keys=(CONF_VL6180X_ID,),
    message="Only one VL6180X sensor platform entry is allowed per hub",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_VL6180X_ID])
    sensors = sensor.sub_sensors(config)
    if range_config := config.get(CONF_RANGE):
        await sensors(CONF_RANGE, hub.set_range_sensor)
        cg.add(hub.set_scaling(range_config[CONF_SCALER]))
    if lux_config := config.get(CONF_LUX):
        await sensors(CONF_LUX, hub.set_lux_sensor)
        gain_code = lux_config[CONF_GAIN].enum_value
        cg.add(hub.set_als_gain(gain_code, ALS_GAIN_FACTORS[gain_code]))
        cg.add_optional(
            hub.set_lux_update_interval, lux_config.get(CONF_UPDATE_INTERVAL)
        )
