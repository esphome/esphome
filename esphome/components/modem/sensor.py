import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY,
    CONF_ALTITUDE,
    CONF_COURSE,
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_SPEED,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_SPEED,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_MEASUREMENT_ANGLE,
    UNIT_DECIBEL_MILLIWATT,
    UNIT_DEGREES,
    UNIT_KILOMETER_PER_HOUR,
    UNIT_METER,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from . import (
    CONF_MODEM_ID,
    GNSS_MODELS,
    ModemComponent,
    request_gnss_power,
    request_polling,
    require_model,
)

DEPENDENCIES = ["modem"]

CONF_BER = "ber"
CONF_RSSI = "rssi"

_GNSS_SENSORS = (
    CONF_LATITUDE,
    CONF_LONGITUDE,
    CONF_ALTITUDE,
    CONF_SPEED,
    CONF_COURSE,
    CONF_ACCURACY,
)

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
        # Same decimals and state classes as the gps component
        cv.Optional(CONF_LATITUDE): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=6,
            icon="mdi:latitude",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_LONGITUDE): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=6,
            icon="mdi:longitude",
            state_class=STATE_CLASS_MEASUREMENT_ANGLE,
        ),
        cv.Optional(CONF_ALTITUDE): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=1,
            icon="mdi:map-marker-up",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_SPEED): sensor.sensor_schema(
            unit_of_measurement=UNIT_KILOMETER_PER_HOUR,
            accuracy_decimals=1,
            icon="mdi:speedometer",
            device_class=DEVICE_CLASS_SPEED,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        cv.Optional(CONF_COURSE): sensor.sensor_schema(
            unit_of_measurement=UNIT_DEGREES,
            accuracy_decimals=1,
            icon="mdi:compass",
            state_class=STATE_CLASS_MEASUREMENT_ANGLE,
        ),
        # A rough radius derived from the HDOP
        cv.Optional(CONF_ACCURACY): sensor.sensor_schema(
            unit_of_measurement=UNIT_METER,
            accuracy_decimals=1,
            icon="mdi:map-marker-radius",
            state_class=STATE_CLASS_MEASUREMENT,
        ),
    }
)

FINAL_VALIDATE_SCHEMA = require_model(_GNSS_SENSORS, GNSS_MODELS)


async def to_code(config: ConfigType) -> None:
    signal = CONF_RSSI in config or CONF_BER in config
    gnss = any(key in config for key in _GNSS_SENSORS)
    if not (signal or gnss):
        return
    hub = await cg.get_variable(config[CONF_MODEM_ID])
    request_polling(hub)
    sensors = sensor.sub_sensors(config)
    if signal:
        cg.add_define("USE_MODEM_SIGNAL")
        await sensors(CONF_RSSI, hub.set_rssi_sensor)
        await sensors(CONF_BER, hub.set_ber_sensor)
    if gnss:
        cg.add_define("USE_MODEM_GNSS")
        request_gnss_power()
        await sensors(CONF_LATITUDE, hub.set_latitude_sensor)
        await sensors(CONF_LONGITUDE, hub.set_longitude_sensor)
        await sensors(CONF_ALTITUDE, hub.set_altitude_sensor)
        await sensors(CONF_SPEED, hub.set_speed_sensor)
        await sensors(CONF_COURSE, hub.set_course_sensor)
        await sensors(CONF_ACCURACY, hub.set_accuracy_sensor)
