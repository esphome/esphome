import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_EMPTY,
    UNIT_PERCENT,
    ICON_GAUGE,
    CONF_TYPE,
)
from ..climate import pid_ns, PIDClimate

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
}

CONF_CLIMATE_ID = "climate_id"
CONFIG_SCHEMA = (
    sensor.sensor_schema(UNIT_PERCENT, ICON_GAUGE, 1, DEVICE_CLASS_EMPTY)
    .extend(
        {
            cv.GenerateID(): cv.declare_id(PIDClimateSensor),
            cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(PIDClimate),
            cv.Required(CONF_TYPE): cv.enum(PID_CLIMATE_SENSOR_TYPES, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def to_code(config):
    parent = yield cg.get_variable(config[CONF_CLIMATE_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    yield sensor.register_sensor(var, config)
    yield cg.register_component(var, config)

    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
