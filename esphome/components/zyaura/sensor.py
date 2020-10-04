import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_CLOCK_PIN, CONF_DATA_PIN, \
    CONF_CO2, CONF_TEMPERATURE, CONF_HUMIDITY, \
    UNIT_PARTS_PER_MILLION, UNIT_CELSIUS, UNIT_PERCENT, \
    ICON_MOLECULE_CO2, ICON_THERMOMETER, ICON_WATER_PERCENT
from esphome.cpp_helpers import gpio_pin_expression

zyaura_ns = cg.esphome_ns.namespace('zyaura')
ZyAuraSensor = zyaura_ns.class_('ZyAuraSensor', cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ZyAuraSensor),
    cv.Required(CONF_CLOCK_PIN): cv.All(pins.internal_gpio_input_pin_schema,
                                        pins.validate_has_interrupt),
    cv.Required(CONF_DATA_PIN): cv.All(pins.internal_gpio_input_pin_schema,
                                       pins.validate_has_interrupt),
    cv.Optional(CONF_CO2): sensor.sensor_schema(UNIT_PARTS_PER_MILLION, ICON_MOLECULE_CO2, 0),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(UNIT_PERCENT, ICON_WATER_PERCENT, 1),
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    pin_clock = yield gpio_pin_expression(config[CONF_CLOCK_PIN])
    cg.add(var.set_pin_clock(pin_clock))
    pin_data = yield gpio_pin_expression(config[CONF_DATA_PIN])
    cg.add(var.set_pin_data(pin_data))

    if CONF_CO2 in config:
        sens = yield sensor.new_sensor(config[CONF_CO2])
        cg.add(var.set_co2_sensor(sens))
    if CONF_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_TEMPERATURE])
        cg.add(var.set_temperature_sensor(sens))
    if CONF_HUMIDITY in config:
        sens = yield sensor.new_sensor(config[CONF_HUMIDITY])
        cg.add(var.set_humidity_sensor(sens))
