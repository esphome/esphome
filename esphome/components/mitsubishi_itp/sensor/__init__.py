import esphome.config_validation as cv
from esphome.components import (
    sensor,
)
from esphome.const import (
    CONF_OUTDOOR_TEMPERATURE,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_HUMIDITY,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_HERTZ,
    UNIT_PERCENT,
)
from esphome.core import coroutine
from ...mitsubishi_itp import (
    mitsubishi_itp_ns,
    sensors_to_config_schema,
    sensors_to_code,
)

CONF_THERMOSTAT_HUMIDITY = "thermostat_humidity"
CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"

CompressorFrequencySensor = mitsubishi_itp_ns.class_(
    "CompressorFrequencySensor", sensor.Sensor
)
OutdoorTemperatureSensor = mitsubishi_itp_ns.class_(
    "OutdoorTemperatureSensor", sensor.Sensor
)
ThermostatHumiditySensor = mitsubishi_itp_ns.class_(
    "ThermostatHumiditySensor", sensor.Sensor
)
ThermostatTemperatureSensor = mitsubishi_itp_ns.class_(
    "ThermostatTemperatureSensor", sensor.Sensor
)

# TODO Storing the registration function here seems weird, but I can't figure out how to determine schema type later
SENSORS = dict[str, cv.Schema](
    {
        "compressor_frequency": sensor.sensor_schema(
            CompressorFrequencySensor,
            unit_of_measurement=UNIT_HERTZ,
            device_class=DEVICE_CLASS_FREQUENCY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        CONF_OUTDOOR_TEMPERATURE: sensor.sensor_schema(
            OutdoorTemperatureSensor,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
            icon="mdi:sun-thermometer-outline",
        ),
        CONF_THERMOSTAT_HUMIDITY: sensor.sensor_schema(
            ThermostatHumiditySensor,
            unit_of_measurement=UNIT_PERCENT,
            device_class=DEVICE_CLASS_HUMIDITY,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=0,
        ),
        CONF_THERMOSTAT_TEMPERATURE: sensor.sensor_schema(
            ThermostatTemperatureSensor,
            unit_of_measurement=UNIT_CELSIUS,
            device_class=DEVICE_CLASS_TEMPERATURE,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=1,
        ),
    }
)

CONFIG_SCHEMA = sensors_to_config_schema(SENSORS)


@coroutine
async def to_code(config):
    await sensors_to_code(config, SENSORS, sensor.register_sensor)
