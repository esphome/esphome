from esphome.automation import parent_ref
import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_MODEL,
    DEVICE_CLASS_TEMPERATURE,
    ICON_PERCENT,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from .. import (
    AQUA_PUBLISHER,
    CONF_SYSTA_BUS_ID,
    CONF_SYSTASOLAR_AQUA,
    DUMP_PUBLISHER,
    DUMP_TAG,
    MESSAGE,
    child_schema,
    systa_bus_ns,
)

AquaSensor = systa_bus_ns.enum("AquaSensor", is_class=True)

CONF_TEMPERATURE_TSA = "temperature_tsa"
CONF_TEMPERATURE_TSE = "temperature_tse"
CONF_TEMPERATURE_TWU = "temperature_twu"
CONF_TEMPERATURE_TW2 = "temperature_tw2"
CONF_PUMP_SPEED = "pump_speed"

_TEMPERATURE_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    icon=ICON_THERMOMETER,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)
_PUMP_SPEED_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_PERCENT,
    icon=ICON_PERCENT,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
)

# Sensors per model, keyed by config key
MODEL_SENSORS = {
    CONF_SYSTASOLAR_AQUA: {
        CONF_TEMPERATURE_TSA: _TEMPERATURE_SCHEMA,
        CONF_TEMPERATURE_TSE: _TEMPERATURE_SCHEMA,
        CONF_TEMPERATURE_TWU: _TEMPERATURE_SCHEMA,
        CONF_TEMPERATURE_TW2: _TEMPERATURE_SCHEMA,
        CONF_PUMP_SPEED: _PUMP_SPEED_SCHEMA,
    },
}

# Aqua sensor data: config key -> (dump name, value decoded from the message)
_AQUA_VALUES = {
    CONF_TEMPERATURE_TSA: (
        AquaSensor.AQUA_SENSOR_TEMPERATURE_TSA,
        systa_bus_ns.aqua_temperature(MESSAGE, 4),
    ),
    CONF_TEMPERATURE_TSE: (
        AquaSensor.AQUA_SENSOR_TEMPERATURE_TSE,
        systa_bus_ns.aqua_temperature(MESSAGE, 6),
    ),
    CONF_TEMPERATURE_TWU: (
        AquaSensor.AQUA_SENSOR_TEMPERATURE_TWU,
        systa_bus_ns.aqua_temperature(MESSAGE, 8),
    ),
    CONF_TEMPERATURE_TW2: (
        AquaSensor.AQUA_SENSOR_TEMPERATURE_TW2,
        systa_bus_ns.aqua_temperature(MESSAGE, 10),
    ),
    CONF_PUMP_SPEED: (AquaSensor.AQUA_SENSOR_PUMP_SPEED, MESSAGE[12]),
}

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_SYSTASOLAR_AQUA: child_schema(
            "sensor",
            {
                cv.Optional(key): schema
                for key, schema in MODEL_SENSORS[CONF_SYSTASOLAR_AQUA].items()
            },
        ),
    },
    key=CONF_MODEL,
    lower=True,
    space="_",
)


async def to_code(config: ConfigType) -> None:
    systa_bus = await cg.get_variable(config[CONF_SYSTA_BUS_ID])
    # The schema only lets a model's own keys through, so unconfigured ones are skipped here
    sensors = sensor.sub_sensors(config)
    for key, (which, value) in _AQUA_VALUES.items():
        if (var := await sensors(key, None)) is None:
            continue
        AQUA_PUBLISHER.add(systa_bus, parent_ref(var).publish_state(value))
        DUMP_PUBLISHER.add(
            systa_bus, systa_bus_ns.dump_aqua_sensor(DUMP_TAG, parent_ref(var), which)
        )
