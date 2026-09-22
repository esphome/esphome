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
    "RESULT": PIDClimateSensorType.PID_SENSOR_TYPE_RESULT,
    "ERROR": PIDClimateSensorType.PID_SENSOR_TYPE_ERROR,
    "PROPORTIONAL": PIDClimateSensorType.PID_SENSOR_TYPE_PROPORTIONAL,
    "INTEGRAL": PIDClimateSensorType.PID_SENSOR_TYPE_INTEGRAL,
    "DERIVATIVE": PIDClimateSensorType.PID_SENSOR_TYPE_DERIVATIVE,
    "HEAT": PIDClimateSensorType.PID_SENSOR_TYPE_HEAT,
    "COOL": PIDClimateSensorType.PID_SENSOR_TYPE_COOL,
    "KP": PIDClimateSensorType.PID_SENSOR_TYPE_KP,
    "KI": PIDClimateSensorType.PID_SENSOR_TYPE_KI,
    "KD": PIDClimateSensorType.PID_SENSOR_TYPE_KD,
    "DEADBAND_THRESHOLD_HIGH": PIDClimateSensorType.PID_SENSOR_TYPE_DEADBAND_THRESHOLD_HIGH,
    "DEADBAND_THRESHOLD_LOW": PIDClimateSensorType.PID_SENSOR_TYPE_DEADBAND_THRESHOLD_LOW,
    "KP_DEADBAND_MULTIPLIER": PIDClimateSensorType.PID_SENSOR_TYPE_KP_DEADBAND_MULTIPLIER,
    "KI_DEADBAND_MULTIPLIER": PIDClimateSensorType.PID_SENSOR_TYPE_KI_DEADBAND_MULTIPLIER,
    "KD_DEADBAND_MULTIPLIER": PIDClimateSensorType.PID_SENSOR_TYPE_KD_DEADBAND_MULTIPLIER,
}

DEADBAND_THRESHOLD_SENSOR_TYPES = (
    PIDClimateSensorType.PID_SENSOR_TYPE_DEADBAND_THRESHOLD_HIGH,
    PIDClimateSensorType.PID_SENSOR_TYPE_DEADBAND_THRESHOLD_LOW,
)

DEADBAND_MULTIPLIER_SENSOR_TYPES = (
    PIDClimateSensorType.PID_SENSOR_TYPE_KP_DEADBAND_MULTIPLIER,
    PIDClimateSensorType.PID_SENSOR_TYPE_KI_DEADBAND_MULTIPLIER,
    PIDClimateSensorType.PID_SENSOR_TYPE_KD_DEADBAND_MULTIPLIER,
)


def set_default_unit_of_measurement(config: ConfigType) -> ConfigType:
    sensor_type = config[CONF_TYPE]
    if sensor_type in DEADBAND_THRESHOLD_SENSOR_TYPES:
        config.setdefault(CONF_UNIT_OF_MEASUREMENT, UNIT_CELSIUS)
    elif sensor_type in DEADBAND_MULTIPLIER_SENSOR_TYPES:
        config.setdefault(CONF_UNIT_OF_MEASUREMENT, UNIT_EMPTY)
    else:
        config.setdefault(CONF_UNIT_OF_MEASUREMENT, UNIT_PERCENT)
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
            cv.Required(CONF_TYPE): cv.enum(PID_CLIMATE_SENSOR_TYPES, upper=True),
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
