import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_TYPE,
    ICON_THERMOMETER,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from ..climate import EquithermClimate, equitherm_climate_ns

EquithermClimateSensor = equitherm_climate_ns.class_(
    "EquithermClimateSensor", sensor.Sensor, cg.Component
)
EquithermClimateSensorType = equitherm_climate_ns.enum("EquithermClimateSensorType")

EQUITHERM_CLIMATE_SENSOR_TYPES = {
    "flow_curve": EquithermClimateSensorType.EQUITHERM_SENSOR_TYPE_FLOW_CURVE,
    "flow_final": EquithermClimateSensorType.EQUITHERM_SENSOR_TYPE_FLOW_FINAL,
    "pid_correction": EquithermClimateSensorType.EQUITHERM_SENSOR_TYPE_PID_CORRECTION,
}

CONF_CLIMATE_ID = "climate_id"

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        EquithermClimateSensor,
        unit_of_measurement=UNIT_CELSIUS,
        icon=ICON_THERMOMETER,
        accuracy_decimals=1,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(EquithermClimate),
            cv.Required(CONF_TYPE): cv.enum(EQUITHERM_CLIMATE_SENSOR_TYPES, lower=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_CLIMATE_ID])
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(parent))
    cg.add(var.set_type(config[CONF_TYPE]))
