import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_ICON,
    CONF_TYPE,
    CONF_UNIT_OF_MEASUREMENT,
    ICON_SCALE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_EMPTY,
)

from ..climate import EquithermClimate, equitherm_ns

EquithermSensor = equitherm_ns.class_("EquithermSensor", sensor.Sensor, cg.Component)
EquithermSensorType = equitherm_ns.enum("EquithermSensorType")

EQUITHERM_SENSOR_TYPES = {
    "base_curve_output": EquithermSensorType.EQUITHERM_SENSOR_TYPE_BASE_CURVE_OUTPUT,
    "final_flow_setpoint": EquithermSensorType.EQUITHERM_SENSOR_TYPE_FINAL_FLOW_SETPOINT,
    "raw_pid_correction": EquithermSensorType.EQUITHERM_SENSOR_TYPE_RAW_PID_CORRECTION,
    "gain_scheduling_ratio": EquithermSensorType.EQUITHERM_SENSOR_TYPE_GAIN_SCHEDULING_RATIO,
    "scaled_correction": EquithermSensorType.EQUITHERM_SENSOR_TYPE_SCALED_CORRECTION,
}

CONF_CLIMATE_ID = "climate_id"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        EquithermSensor,
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(EquithermClimate),
            cv.Required(CONF_TYPE): cv.enum(EQUITHERM_SENSOR_TYPES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    # Apply type-specific defaults for gain_scheduling_ratio
    if config[CONF_TYPE] == "gain_scheduling_ratio":
        config.setdefault(CONF_UNIT_OF_MEASUREMENT, UNIT_EMPTY)
        config.setdefault(CONF_ICON, ICON_SCALE)
        config.setdefault(CONF_ACCURACY_DECIMALS, 2)

    parent = await cg.get_variable(config[CONF_CLIMATE_ID])
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
