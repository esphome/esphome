import esphome.codegen as cg
from esphome.components import sensor
from esphome.components.const import CONF_CLIMATE_ID
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    ICON_GAUGE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
    UNIT_PERCENT,
)
from esphome.types import ConfigType

from ..climate import PIDClimate, pid_ns

PIDClimateSensor = pid_ns.class_("PIDClimateSensor", sensor.Sensor, cg.Component)
PIDClimateSensorType = pid_ns.enum("PIDClimateSensorType")

PID_CLIMATE_SENSOR_TYPES = {
    "RESULT": (PIDClimateSensorType.PID_SENSOR_TYPE_RESULT, UNIT_PERCENT),
    "ERROR": (PIDClimateSensorType.PID_SENSOR_TYPE_ERROR, UNIT_PERCENT),
    "PROPORTIONAL": (PIDClimateSensorType.PID_SENSOR_TYPE_PROPORTIONAL, UNIT_PERCENT),
    "INTEGRAL": (PIDClimateSensorType.PID_SENSOR_TYPE_INTEGRAL, UNIT_PERCENT),
    "DERIVATIVE": (PIDClimateSensorType.PID_SENSOR_TYPE_DERIVATIVE, UNIT_PERCENT),
    "HEAT": (PIDClimateSensorType.PID_SENSOR_TYPE_HEAT, UNIT_PERCENT),
    "COOL": (PIDClimateSensorType.PID_SENSOR_TYPE_COOL, UNIT_PERCENT),
    "KP": (PIDClimateSensorType.PID_SENSOR_TYPE_KP, UNIT_PERCENT),
    "KI": (PIDClimateSensorType.PID_SENSOR_TYPE_KI, UNIT_PERCENT),
    "KD": (PIDClimateSensorType.PID_SENSOR_TYPE_KD, UNIT_PERCENT),
    "DEADBAND_THRESHOLD_HIGH": (
        PIDClimateSensorType.PID_SENSOR_TYPE_DEADBAND_THRESHOLD_HIGH,
        UNIT_CELSIUS,
    ),
    "DEADBAND_THRESHOLD_LOW": (
        PIDClimateSensorType.PID_SENSOR_TYPE_DEADBAND_THRESHOLD_LOW,
        UNIT_CELSIUS,
    ),
    "KP_DEADBAND_MULTIPLIER": (
        PIDClimateSensorType.PID_SENSOR_TYPE_KP_DEADBAND_MULTIPLIER,
        UNIT_EMPTY,
    ),
    "KI_DEADBAND_MULTIPLIER": (
        PIDClimateSensorType.PID_SENSOR_TYPE_KI_DEADBAND_MULTIPLIER,
        UNIT_EMPTY,
    ),
    "KD_DEADBAND_MULTIPLIER": (
        PIDClimateSensorType.PID_SENSOR_TYPE_KD_DEADBAND_MULTIPLIER,
        UNIT_EMPTY,
    ),
}

PID_CLIMATE_SENSOR_ENUMS = {
    sensor_type: sensor_config[0]
    for sensor_type, sensor_config in PID_CLIMATE_SENSOR_TYPES.items()
}


def set_default_unit_of_measurement(config: ConfigType) -> ConfigType:
    sensor_type = config[CONF_TYPE]
    config.setdefault(
        CONF_UNIT_OF_MEASUREMENT, PID_CLIMATE_SENSOR_TYPES[sensor_type][1]
    )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        PIDClimateSensor,
        icon=ICON_GAUGE,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(PIDClimate),
            cv.Required(CONF_TYPE): cv.enum(PID_CLIMATE_SENSOR_ENUMS, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    set_default_unit_of_measurement,
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_CLIMATE_ID])
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
