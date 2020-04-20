import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import CONF_ID, CONF_PIN, \
    CONF_RESOLUTION, CONF_UNIT_OF_MEASUREMENT, UNIT_CELSIUS, \
    CONF_ICON, ICON_THERMOMETER, CONF_ACCURACY_DECIMALS

MULTI_CONF = True
AUTO_LOAD = ['sensor']

CONF_ONE_WIRE_ID = 'one_wire_id'
CONF_AUTO_SETUP_SENSORS = 'auto_setup_sensors'
CONF_SENSOR_NAME_TEMPLATE = 'sensor_name_template'
SENSOR_NAME_TEMPLATE_DEFAULT = '%s.%s'

dallas_ns = cg.esphome_ns.namespace('dallas')
DallasComponent = dallas_ns.class_('DallasComponent', cg.PollingComponent)
ESPOneWire = dallas_ns.class_('ESPOneWire')

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DallasComponent),
    cv.GenerateID(CONF_ONE_WIRE_ID): cv.declare_id(ESPOneWire),
    cv.Required(CONF_PIN): pins.gpio_input_pin_schema,
    cv.Optional(CONF_AUTO_SETUP_SENSORS, default=False): cv.boolean,
    cv.Optional(CONF_SENSOR_NAME_TEMPLATE, default=SENSOR_NAME_TEMPLATE_DEFAULT): cv.string_strict,
    cv.Optional(CONF_RESOLUTION, default=12): cv.int_range(min=9, max=12),
    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_CELSIUS): cv.string_strict,
    cv.Optional(CONF_ICON, default=ICON_THERMOMETER): cv.icon,
    cv.Optional(CONF_ACCURACY_DECIMALS, default=1): cv.int_,
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    pin = yield cg.gpio_pin_expression(config[CONF_PIN])
    one_wire = cg.new_Pvariable(config[CONF_ONE_WIRE_ID], pin)
    var = cg.new_Pvariable(config[CONF_ID], one_wire)
    if CONF_AUTO_SETUP_SENSORS in config:
        cg.add(var.set_auto_setup_sensors(config[CONF_AUTO_SETUP_SENSORS]))
    if CONF_SENSOR_NAME_TEMPLATE in config:
        cg.add(var.set_sensor_name_template(config[CONF_SENSOR_NAME_TEMPLATE]))
    if CONF_RESOLUTION in config:
        cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    if CONF_UNIT_OF_MEASUREMENT in config:
        cg.add(var.set_unit_of_measurement(config[CONF_UNIT_OF_MEASUREMENT]))
    if CONF_ICON in config:
        cg.add(var.set_icon(config[CONF_ICON]))
    if CONF_ACCURACY_DECIMALS in config:
        cg.add(var.set_accuracy_decimals(config[CONF_ACCURACY_DECIMALS]))
    yield cg.register_component(var, config)
