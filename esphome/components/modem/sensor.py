import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY,
    CONF_ALTITUDE,
    CONF_COURSE,
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_MODEL,
    CONF_SPEED,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_DEGREES,
    UNIT_KILOMETER_PER_HOUR,
    UNIT_METER,
    UNIT_PERCENT,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import CONF_MODEM_ID, GNSS_MODELS, ModemComponent, request_polling

DEPENDENCIES = ["modem"]

CONF_BER = "ber"
CONF_RSSI = "rssi"

_GNSS_SENSORS = {
    CONF_LATITUDE: (UNIT_DEGREES, 5, "mdi:latitude"),
    CONF_LONGITUDE: (UNIT_DEGREES, 5, "mdi:longitude"),
    CONF_ALTITUDE: (UNIT_METER, 1, "mdi:map-marker-up"),
    CONF_SPEED: (UNIT_KILOMETER_PER_HOUR, 1, "mdi:speedometer"),
    CONF_COURSE: (UNIT_DEGREES, 1, "mdi:compass"),
    CONF_ACCURACY: (UNIT_METER, 1, "mdi:map-marker-radius"),
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MODEM_ID): cv.use_id(ModemComponent),
        cv.Optional(CONF_RSSI): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MILLIWATT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_BER): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=2,
            icon="mdi:signal",
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        **{
            cv.Optional(key): sensor.sensor_schema(
                unit_of_measurement=unit,
                accuracy_decimals=decimals,
                icon=icon,
                state_class=STATE_CLASS_MEASUREMENT,
            )
            for key, (unit, decimals, icon) in _GNSS_SENSORS.items()
        },
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    if not any(key in config for key in _GNSS_SENSORS):
        return config
    model = fv.get_declaring_config(config[CONF_MODEM_ID])[CONF_MODEL]
    if model not in GNSS_MODELS:
        raise cv.Invalid(
            f"GNSS sensors are not supported on {model}; supported models: {', '.join(GNSS_MODELS)}"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_MODEM_ID])
    request_polling(hub)
    sensors = sensor.sub_sensors(config)
    await sensors(CONF_RSSI, hub.set_rssi_sensor)
    await sensors(CONF_BER, hub.set_ber_sensor)
    gnss = [
        await sensors(CONF_LATITUDE, hub.set_latitude_sensor),
        await sensors(CONF_LONGITUDE, hub.set_longitude_sensor),
        await sensors(CONF_ALTITUDE, hub.set_altitude_sensor),
        await sensors(CONF_SPEED, hub.set_speed_sensor),
        await sensors(CONF_COURSE, hub.set_course_sensor),
        await sensors(CONF_ACCURACY, hub.set_accuracy_sensor),
    ]
    cg.add_define("USE_MODEM_SENSOR")
    if any(s is not None for s in gnss):
        cg.add_define("USE_MODEM_GNSS")
