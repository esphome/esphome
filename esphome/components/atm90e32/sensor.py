import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, spi
from esphome.const import \
    CONF_ID, CONF_POWER, CONF_FREQUENCY, \
    ICON_FLASH, UNIT_HZ, UNIT_VOLT, UNIT_AMPERE, UNIT_WATT

CONF_VOLTAGE_A = 'voltage_a'
CONF_VOLTAGE_B = 'voltage_b'
CONF_VOLTAGE_C = 'voltage_c'
CONF_CURRENT_A = 'current_a'
CONF_CURRENT_B = 'current_b'
CONF_CURRENT_C = 'current_c'
CONF_LINE_FREQ = 'line_freq'
CONF_GAIN_PGA = 'gain_pga'
CONF_GAIN_VOLT_A = 'gain_volt_a'
CONF_GAIN_VOLT_B = 'gain_volt_b'
CONF_GAIN_VOLT_C = 'gain_volt_c'
CONF_GAIN_CT_A = 'gain_ct_a'
CONF_GAIN_CT_B = 'gain_ct_b'
CONF_GAIN_CT_C = 'gain_ct_c'
LINE_FREQS = {
    '50HZ': 50,
    '60HZ': 60,
}
PGA_GAINS = {
    '1X': 0x0,
    '2X': 0x15,
    '4X': 0x2A,
}

atm90e32_ns = cg.esphome_ns.namespace('atm90e32')
ATM90E32Component = atm90e32_ns.class_('ATM90E32Component', cg.PollingComponent, spi.SPIDevice)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ATM90E32Component),
    cv.Optional(CONF_VOLTAGE_A): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_VOLTAGE_B): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_VOLTAGE_C): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_CURRENT_A): sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2),
    cv.Optional(CONF_CURRENT_B): sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2),
    cv.Optional(CONF_CURRENT_C): sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2),
    cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(UNIT_HZ, ICON_FLASH, 1),
    cv.Optional(CONF_POWER): sensor.sensor_schema(UNIT_WATT, ICON_FLASH, 1),
    cv.Required(CONF_LINE_FREQ): cv.enum(LINE_FREQS, upper=True),
    cv.Optional(CONF_GAIN_PGA, default='2X'): cv.enum(PGA_GAINS, upper=True),
    cv.Optional(CONF_GAIN_VOLT_A, default=41820): cv.uint16_t,
    cv.Optional(CONF_GAIN_VOLT_B, default=41820): cv.uint16_t,
    cv.Optional(CONF_GAIN_VOLT_C, default=41820): cv.uint16_t,
    cv.Optional(CONF_GAIN_CT_A, default=25498): cv.uint16_t,
    cv.Optional(CONF_GAIN_CT_B, default=25498): cv.uint16_t,
    cv.Optional(CONF_GAIN_CT_C, default=25498): cv.uint16_t,
}).extend(cv.polling_component_schema('60s')).extend(spi.SPI_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)

    if CONF_VOLTAGE_A in config:
        sens = yield sensor.new_sensor(config[CONF_VOLTAGE_A])
        cg.add(var.set_voltage_sensor_a(sens))
    if CONF_VOLTAGE_B in config:
        sens = yield sensor.new_sensor(config[CONF_VOLTAGE_B])
        cg.add(var.set_voltage_sensor_b(sens))
    if CONF_VOLTAGE_C in config:
        sens = yield sensor.new_sensor(config[CONF_VOLTAGE_C])
        cg.add(var.set_voltage_sensor_c(sens))
    if CONF_CURRENT_A in config:
        sens = yield sensor.new_sensor(config[CONF_CURRENT_A])
        cg.add(var.set_current_sensor_a(sens))
    if CONF_CURRENT_B in config:
        sens = yield sensor.new_sensor(config[CONF_CURRENT_B])
        cg.add(var.set_current_sensor_b(sens))
    if CONF_CURRENT_C in config:
        sens = yield sensor.new_sensor(config[CONF_CURRENT_C])
        cg.add(var.set_current_sensor_c(sens))
    if CONF_FREQUENCY in config:
        sens = yield sensor.new_sensor(config[CONF_FREQUENCY])
        cg.add(var.set_freq_sensor(sens))
    if CONF_POWER in config:
        sens = yield sensor.new_sensor(config[CONF_POWER])
        cg.add(var.set_power_sensor(sens))
    cg.add(var.set_line_freq(config[CONF_LINE_FREQ]))
    if CONF_GAIN_PGA in config:
        cg.add(var.set_pga_gain(config[CONF_GAIN_PGA]))
    if CONF_GAIN_VOLT_A in config:
        cg.add(var.set_volt_a_gain(config[CONF_GAIN_VOLT_A]))
    if CONF_GAIN_VOLT_B in config:
        cg.add(var.set_volt_b_gain(config[CONF_GAIN_VOLT_B]))
    if CONF_GAIN_VOLT_C in config:
        cg.add(var.set_volt_c_gain(config[CONF_GAIN_VOLT_C]))
    if CONF_GAIN_CT_A in config:
        cg.add(var.set_ct_a_gain(config[CONF_GAIN_CT_A]))
    if CONF_GAIN_CT_B in config:
        cg.add(var.set_ct_b_gain(config[CONF_GAIN_CT_B]))
    if CONF_GAIN_CT_C in config:
        cg.add(var.set_ct_c_gain(config[CONF_GAIN_CT_C]))
