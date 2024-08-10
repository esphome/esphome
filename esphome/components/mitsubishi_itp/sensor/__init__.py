import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    sensor,
    binary_sensor,
    text_sensor,
)
from esphome.const import (
    CONF_ID,
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
from ...mitsubishi_itp.climate import (
    CONF_MITSUBISHI_ITP_ID,
    mitsubishi_itp_ns,
    MitsubishiUART,
)

CONF_THERMOSTAT_BATTERY = "thermostat_battery"
CONF_THERMOSTAT_HUMIDITY = "thermostat_humidity"
CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"


CONF_ERROR_CODE = "error_code"
CONF_ISEE_STATUS = "isee_status"

ActualFanSensor = mitsubishi_itp_ns.class_("ActualFanSensor", text_sensor.TextSensor)
CompressorFrequencySensor = mitsubishi_itp_ns.class_(
    "CompressorFrequencySensor", sensor.Sensor
)
DefrostSensor = mitsubishi_itp_ns.class_("DefrostSensor", binary_sensor.BinarySensor)
ErrorCodeSensor = mitsubishi_itp_ns.class_("ErrorCodeSensor", text_sensor.TextSensor)
FilterStatusSensor = mitsubishi_itp_ns.class_(
    "FilterStatusSensor", binary_sensor.BinarySensor
)
ISeeStatusSensor = mitsubishi_itp_ns.class_(
    "ISeeStatusSensor", binary_sensor.BinarySensor
)
OutdoorTemperatureSensor = mitsubishi_itp_ns.class_(
    "OutdoorTemperatureSensor", sensor.Sensor
)
PreheatSensor = mitsubishi_itp_ns.class_("PreheatSensor", binary_sensor.BinarySensor)
StandbySensor = mitsubishi_itp_ns.class_("StandbySensor", binary_sensor.BinarySensor)
ThermostatBatterySensor = mitsubishi_itp_ns.class_(
    "ThermostatBatterySensor", text_sensor.TextSensor
)
ThermostatHumiditySensor = mitsubishi_itp_ns.class_(
    "ThermostatHumiditySensor", sensor.Sensor
)
ThermostatTemperatureSensor = mitsubishi_itp_ns.class_(
    "ThermostatTemperatureSensor", sensor.Sensor
)

# TODO Storing the registration function here seems weird, but I can't figure out how to determine schema type later
SENSORS = dict[str, tuple[cv.Schema, callable]](
    {
        "actual_fan": (
            text_sensor.text_sensor_schema(
                ActualFanSensor,
                icon="mdi:fan",
            ),
            text_sensor.register_text_sensor,
        ),
        "compressor_frequency": (
            sensor.sensor_schema(
                CompressorFrequencySensor,
                unit_of_measurement=UNIT_HERTZ,
                device_class=DEVICE_CLASS_FREQUENCY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            sensor.register_sensor,
        ),
        "defrost": (
            binary_sensor.binary_sensor_schema(
                DefrostSensor, icon="mdi:snowflake-melt"
            ),
            binary_sensor.register_binary_sensor,
        ),
        CONF_ERROR_CODE: (
            text_sensor.text_sensor_schema(
                ErrorCodeSensor, icon="mdi:alert-circle-outline"
            ),
            text_sensor.register_text_sensor,
        ),
        "filter_status": (
            binary_sensor.binary_sensor_schema(
                FilterStatusSensor, device_class="problem", icon="mdi:air-filter"
            ),
            binary_sensor.register_binary_sensor,
        ),
        CONF_ISEE_STATUS: (
            binary_sensor.binary_sensor_schema(ISeeStatusSensor, icon="mdi:eye"),
            binary_sensor.register_binary_sensor,
        ),
        CONF_OUTDOOR_TEMPERATURE: (
            sensor.sensor_schema(
                OutdoorTemperatureSensor,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
                icon="mdi:sun-thermometer-outline",
            ),
            sensor.register_sensor,
        ),
        "preheat": (
            binary_sensor.binary_sensor_schema(PreheatSensor, icon="mdi:heating-coil"),
            binary_sensor.register_binary_sensor,
        ),
        "standby": (
            binary_sensor.binary_sensor_schema(
                StandbySensor, icon="mdi:pause-circle-outline"
            ),
            binary_sensor.register_binary_sensor,
        ),
        CONF_THERMOSTAT_BATTERY: (
            text_sensor.text_sensor_schema(
                ThermostatBatterySensor,
                icon="mdi:battery",
            ),
            text_sensor.register_text_sensor,
        ),
        CONF_THERMOSTAT_HUMIDITY: (
            sensor.sensor_schema(
                ThermostatHumiditySensor,
                unit_of_measurement=UNIT_PERCENT,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            sensor.register_sensor,
        ),
        CONF_THERMOSTAT_TEMPERATURE: (
            sensor.sensor_schema(
                ThermostatTemperatureSensor,
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
            ),
            sensor.register_sensor,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_MITSUBISHI_ITP_ID): cv.use_id(MitsubishiUART),
    }
).extend(
    {
        cv.Optional(sensor_designator): sensor_schema
        for sensor_designator, (
            sensor_schema,
            _,
        ) in SENSORS.items()
    }
)


@coroutine
async def to_code(config):
    muart_component = await cg.get_variable(config[CONF_MITSUBISHI_ITP_ID])

    # Sensors

    for sensor_designator, (
        _,
        registration_function,
    ) in SENSORS.items():
        if sensor_conf := config.get(sensor_designator):
            sensor_component = cg.new_Pvariable(sensor_conf[CONF_ID])

            await registration_function(sensor_component, sensor_conf)

            cg.add(getattr(muart_component, "register_listener")(sensor_component))
