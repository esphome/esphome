import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID, UNIT_PERCENT, ICON_GAUGE
from ..climate import pid_ns, PIDClimate

DallasTemperatureSensor = pid_ns.class_('PIDClimateSensor', sensor.Sensor, cg.Component)

CONF_CLIMATE_ID = 'climate_id'
CONFIG_SCHEMA = sensor.sensor_schema(UNIT_PERCENT, ICON_GAUGE, 1).extend({
    cv.GenerateID(): cv.declare_id(DallasTemperatureSensor),
    cv.GenerateID(CONF_CLIMATE_ID): cv.use_id(PIDClimate),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    parent = yield cg.get_variable(config[CONF_CLIMATE_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    yield sensor.register_sensor(var, config)
    yield cg.register_component(var, config)

    cg.add(var.set_parent(parent))
