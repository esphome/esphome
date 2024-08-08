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
from esphome.components.mitsubishi_itp.climate import (
    CONF_MITSUBISHI_IPT_ID,
    MitsubishiUART,
)
from esphome.core import coroutine

CONF_THERMOSTAT_BATTERY = "thermostat_battery"
CONF_THERMOSTAT_HUMIDITY = "thermostat_humidity"
CONF_THERMOSTAT_TEMPERATURE = "thermostat_temperature"


CONF_ERROR_CODE = "error_code"
CONF_ISEE_STATUS = "isee_status"

# TODO Storing the registration function here seems weird, but I can't figure out how to determine schema type later
SENSORS = dict[str, tuple[cv.Schema, callable]](
    {
        "actual_fan": (
            text_sensor.text_sensor_schema(
                icon="mdi:fan",
            ),
            text_sensor.register_text_sensor,
        ),
        "compressor_frequency": (
            sensor.sensor_schema(
                unit_of_measurement=UNIT_HERTZ,
                device_class=DEVICE_CLASS_FREQUENCY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            sensor.register_sensor,
        ),
        "defrost": (
            binary_sensor.binary_sensor_schema(icon="mdi:snowflake-melt"),
            binary_sensor.register_binary_sensor,
        ),
        CONF_ERROR_CODE: (
            text_sensor.text_sensor_schema(icon="mdi:alert-circle-outline"),
            text_sensor.register_text_sensor,
        ),
        "filter_status": (
            binary_sensor.binary_sensor_schema(
                device_class="problem", icon="mdi:air-filter"
            ),
            binary_sensor.register_binary_sensor,
        ),
        CONF_ISEE_STATUS: (
            binary_sensor.binary_sensor_schema(icon="mdi:eye"),
            binary_sensor.register_binary_sensor,
        ),
        CONF_OUTDOOR_TEMPERATURE: (
            sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=1,
                icon="mdi:sun-thermometer-outline",
            ),
            sensor.register_sensor,
        ),
        "preheat": (
            binary_sensor.binary_sensor_schema(icon="mdi:heating-coil"),
            binary_sensor.register_binary_sensor,
        ),
        "standby": (
            binary_sensor.binary_sensor_schema(icon="mdi:pause-circle-outline"),
            binary_sensor.register_binary_sensor,
        ),
        CONF_THERMOSTAT_BATTERY: (
            text_sensor.text_sensor_schema(
                icon="mdi:battery",
            ),
            text_sensor.register_text_sensor,
        ),
        CONF_THERMOSTAT_HUMIDITY: (
            sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
                accuracy_decimals=0,
            ),
            sensor.register_sensor,
        ),
        CONF_THERMOSTAT_TEMPERATURE: (
            sensor.sensor_schema(
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
        cv.GenerateID(CONF_MITSUBISHI_IPT_ID): cv.use_id(MitsubishiUART),
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
    muart_component = await cg.get_variable(config[CONF_MITSUBISHI_IPT_ID])

    # Sensors

    for sensor_designator, (
        _,
        registration_function,
    ) in SENSORS.items():
        if sensor_conf := config.get(sensor_designator):
            sensor_component = cg.new_Pvariable(sensor_conf[CONF_ID])

            await registration_function(sensor_component, sensor_conf)

            cg.add(
                getattr(muart_component, f"set_{sensor_designator}_sensor")(
                    sensor_component
                )
            )
