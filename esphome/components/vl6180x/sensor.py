import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_DISTANCE,
    CONF_GAIN,
    CONF_ILLUMINANCE,
    CONF_INTEGRATION_TIME,
    DEVICE_CLASS_DISTANCE,
    DEVICE_CLASS_ILLUMINANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_LUX,
    UNIT_MILLIMETER,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_VL6180X_ID, DOMAIN, VL6180XComponent

DEPENDENCIES = ["vl6180x"]

CONF_SCALER = "scaler"

# Gain factor to the SYSALS_ANALOGUE_GAIN code
ALS_GAINS = {1: 6, 1.25: 5, 1.67: 4, 2.5: 3, 5: 2, 10: 1, 20: 0, 40: 7}

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_VL6180X_ID): cv.use_id(VL6180XComponent),
            cv.Optional(CONF_DISTANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_MILLIMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_DISTANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {cv.Optional(CONF_SCALER, default=1): cv.one_of(1, 2, 3, int=True)}
            ),
            cv.Optional(CONF_ILLUMINANCE): sensor.sensor_schema(
                unit_of_measurement=UNIT_LUX,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_ILLUMINANCE,
                state_class=STATE_CLASS_MEASUREMENT,
            ).extend(
                {
                    cv.Optional(CONF_GAIN, default=1): cv.enum(ALS_GAINS),
                    cv.Optional(CONF_INTEGRATION_TIME, default="100ms"): cv.All(
                        cv.positive_time_period_milliseconds,
                        cv.Range(
                            min=cv.TimePeriod(milliseconds=1),
                            max=cv.TimePeriod(milliseconds=512),
                        ),
                    ),
                }
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_DISTANCE, CONF_ILLUMINANCE),
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
    if (distance_config := config.get(CONF_DISTANCE)) is not None:
        await sensors(CONF_DISTANCE, hub.set_distance_sensor)
        cg.add(hub.set_scaler(distance_config[CONF_SCALER]))
    if (illuminance_config := config.get(CONF_ILLUMINANCE)) is not None:
        await sensors(CONF_ILLUMINANCE, hub.set_illuminance_sensor)
        gain = illuminance_config[CONF_GAIN]
        time_ms = illuminance_config[CONF_INTEGRATION_TIME].total_milliseconds
        # The datasheet scale is 0.32 lux per count at 1x gain and 100 ms
        cg.add(hub.set_als(gain.enum_value, time_ms, 32.0 / (float(gain) * time_ms)))
