import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

from .. import CONF_PARENT_ID, FendtCaravanHubBase, fendt_caravan_ns

FendtSensor = fendt_caravan_ns.class_("FendtSensor", sensor.Sensor)
CONF_TEMP_IN = "temp_in"
CONF_TEMP_OUT = "temp_out"
CONF_WATER_LEVEL = "water_level"


def _sensor_schema(
    unit_of_measurement: str = cv.UNDEFINED,
    accuracy_decimals: int = cv.UNDEFINED,
    device_class: str = cv.UNDEFINED,
    state_class: str = cv.UNDEFINED,
) -> cv.Schema:
    return sensor.sensor_schema(
        FendtSensor,
        unit_of_measurement=unit_of_measurement,
        accuracy_decimals=accuracy_decimals,
        device_class=device_class,
        state_class=state_class,
    ).extend(
        {
            cv.Required(CONF_PARENT_ID): cv.use_id(FendtCaravanHubBase),
        }
    )


CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_TEMP_IN: _sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ),
        CONF_TEMP_OUT: _sensor_schema(
            unit_of_measurement=UNIT_CELSIUS,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_TEMPERATURE,
        ),
        CONF_WATER_LEVEL: _sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=1,
            state_class=STATE_CLASS_MEASUREMENT,
            device_class=DEVICE_CLASS_HUMIDITY,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_PARENT_ID])
    var = await sensor.new_sensor(config)
    await cg.register_parented(var, parent)
    cg.add(getattr(parent, f"set_{config[CONF_TYPE]}_sensor")(var))
