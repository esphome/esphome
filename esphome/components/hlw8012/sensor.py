import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_CHANGE_MODE_EVERY, CONF_CURRENT, \
    CONF_CURRENT_RESISTOR, CONF_ID, CONF_POWER, CONF_SEL_PIN, CONF_VOLTAGE, CONF_VOLTAGE_DIVIDER, \
    ICON_FLASH, UNIT_VOLT, UNIT_AMPERE, UNIT_WATT

AUTO_LOAD = ['pulse_counter']

hlw8012_ns = cg.esphome_ns.namespace('hlw8012')
HLW8012Component = hlw8012_ns.class_('HLW8012Component', cg.PollingComponent)

CONF_CF1_PIN = 'cf1_pin'
CONF_CF_PIN = 'cf_pin'
CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(HLW8012Component),
    cv.Required(CONF_SEL_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_CF_PIN): cv.All(pins.internal_gpio_input_pullup_pin_schema,
                                     pins.validate_has_interrupt),
    cv.Required(CONF_CF1_PIN): cv.All(pins.internal_gpio_input_pullup_pin_schema,
                                      pins.validate_has_interrupt),

    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2),
    cv.Optional(CONF_POWER): sensor.sensor_schema(UNIT_WATT, ICON_FLASH, 1),

    cv.Optional(CONF_CURRENT_RESISTOR, default=0.001): cv.resistance,
    cv.Optional(CONF_VOLTAGE_DIVIDER, default=2351): cv.positive_float,
    cv.Optional(CONF_CHANGE_MODE_EVERY, default=8): cv.All(cv.uint32_t, cv.Range(min=1)),
}).extend(cv.polling_component_schema('60s'))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)

    sel = yield cg.gpio_pin_expression(config[CONF_SEL_PIN])
    cg.add(var.set_sel_pin(sel))
    cf = yield cg.gpio_pin_expression(config[CONF_CF_PIN])
    cg.add(var.set_cf_pin(cf))
    cf1 = yield cg.gpio_pin_expression(config[CONF_CF1_PIN])
    cg.add(var.set_cf1_pin(cf1))

    if CONF_VOLTAGE in config:
        sens = yield sensor.new_sensor(config[CONF_VOLTAGE])
        cg.add(var.set_voltage_sensor(sens))
    if CONF_CURRENT in config:
        sens = yield sensor.new_sensor(config[CONF_CURRENT])
        cg.add(var.set_current_sensor(sens))
    if CONF_POWER in config:
        sens = yield sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))
    cg.add(var.set_current_resistor(config[CONF_CURRENT_RESISTOR]))
    cg.add(var.set_voltage_divider(config[CONF_VOLTAGE_DIVIDER]))
    cg.add(var.set_change_mode_every(config[CONF_CHANGE_MODE_EVERY]))
