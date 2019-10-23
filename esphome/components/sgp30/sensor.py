import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c, sensor
from esphome.const import CONF_ID, ICON_RADIATOR, UNIT_PARTS_PER_MILLION, \
    UNIT_PARTS_PER_BILLION, ICON_PERIODIC_TABLE_CO2

DEPENDENCIES = ['i2c']

sgp30_ns = cg.esphome_ns.namespace('sgp30')
SGP30Component = sgp30_ns.class_('SGP30Component', cg.PollingComponent, i2c.I2CDevice)

CONF_ECO2 = 'eco2'
CONF_TVOC = 'tvoc'
CONF_BASELINE = 'baseline'
CONF_UPTIME = 'uptime'
CONF_COMPENSATION = 'compensation'
CONF_HUMIDITY_SOURCE = 'humidity_source'
CONF_TEMPERATURE_SOURCE = 'temperature_source'

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(SGP30Component),
    cv.Required(CONF_ECO2): sensor.sensor_schema(UNIT_PARTS_PER_MILLION,
                                                 ICON_PERIODIC_TABLE_CO2, 0),
    cv.Required(CONF_TVOC): sensor.sensor_schema(UNIT_PARTS_PER_BILLION, ICON_RADIATOR, 0),
    cv.Optional(CONF_BASELINE): cv.hex_uint16_t,
    cv.Optional(CONF_COMPENSATION): cv.Schema({
        cv.Required(CONF_HUMIDITY_SOURCE): cv.use_id(sensor.Sensor),
        cv.Required(CONF_TEMPERATURE_SOURCE): cv.use_id(sensor.Sensor)
    }),
}).extend(cv.polling_component_schema('60s')).extend(i2c.i2c_device_schema(0x58))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield i2c.register_i2c_device(var, config)

    if CONF_ECO2 in config:
        sens = yield sensor.new_sensor(config[CONF_ECO2])
        cg.add(var.set_eco2_sensor(sens))

    if CONF_TVOC in config:
        sens = yield sensor.new_sensor(config[CONF_TVOC])
        cg.add(var.set_tvoc_sensor(sens))

    if CONF_BASELINE in config:
        cg.add(var.set_baseline(config[CONF_BASELINE]))

    if CONF_COMPENSATION in config:
        compensation_config = config[CONF_COMPENSATION]
        sens = yield cg.get_variable(compensation_config[CONF_HUMIDITY_SOURCE])
        cg.add(var.set_humidity_sensor(sens))
        sens = yield cg.get_variable(compensation_config[CONF_TEMPERATURE_SOURCE])
        cg.add(var.set_temperature_sensor(sens))
