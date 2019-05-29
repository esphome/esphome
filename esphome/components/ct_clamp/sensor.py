import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, voltage_sampler
from esphome.const import CONF_SENSOR, CONF_ID, CONF_PIN, ICON_FLASH, UNIT_AMPERE

AUTO_LOAD = ['voltage_sampler']

CONF_SAMPLE_SIZE = 'sample_size'

ct_clamp_ns = cg.esphome_ns.namespace('ct_clamp')
CTClampSensor = ct_clamp_ns.class_('CTClampSensor', sensor.Sensor, cg.PollingComponent)

CONFIG_SCHEMA = sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2).extend({
    cv.GenerateID(): cv.declare_id(CTClampSensor),
    cv.Required(CONF_SENSOR): cv.use_id(voltage_sampler.VoltageSampler),
    cv.Optional(CONF_SAMPLE_SIZE, default=1480): cv.positive_int,
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)

    sens = yield cg.get_variable(config[CONF_SENSOR])

    cg.add(var.set_source(sens))
    cg.add(var.set_sample_size(config[CONF_SAMPLE_SIZE]))
