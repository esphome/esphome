import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, spi
from esphome.const import \
    CONF_ID, CONF_VOLTAGE, CONF_CURRENT, CONF_POWER, CONF_POWER_FACTOR, CONF_FREQUENCY, \
    ICON_FLASH, ICON_LIGHTBULB, ICON_CURRENT_AC, ICON_THERMOMETER, \
    UNIT_HERTZ, UNIT_VOLT, UNIT_AMPERE, UNIT_WATT, UNIT_EMPTY, UNIT_CELSIUS, UNIT_VOLT_AMPS_REACTIVE

CONF_PHASE_A = 'phase_a'
CONF_PHASE_B = 'phase_b'
CONF_PHASE_C = 'phase_c'

CONF_REACTIVE_POWER = 'reactive_power'
CONF_LINE_FREQUENCY = 'line_frequency'
CONF_CHIP_TEMPERATURE = 'chip_temperature'
CONF_GAIN_PGA = 'gain_pga'
CONF_CURRENT_PHASES = 'current_phases'
CONF_GAIN_VOLTAGE = 'gain_voltage'
CONF_GAIN_CT = 'gain_ct'
LINE_FREQS = {
    '50HZ': 50,
    '60HZ': 60,
}
CURRENT_PHASES = {
    '2': 2,
    '3': 3,
}
PGA_GAINS = {
    '1X': 0x0,
    '2X': 0x15,
    '4X': 0x2A,
}

atm90e32_ns = cg.esphome_ns.namespace('atm90e32')
ATM90E32Component = atm90e32_ns.class_('ATM90E32Component', cg.PollingComponent, spi.SPIDevice)

ATM90E32_PHASE_SCHEMA = cv.Schema({
    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 2),
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(UNIT_AMPERE, ICON_CURRENT_AC, 2),
    cv.Optional(CONF_POWER): sensor.sensor_schema(UNIT_WATT, ICON_FLASH, 2),
    cv.Optional(CONF_REACTIVE_POWER): sensor.sensor_schema(UNIT_VOLT_AMPS_REACTIVE,
                                                           ICON_LIGHTBULB, 2),
    cv.Optional(CONF_POWER_FACTOR): sensor.sensor_schema(UNIT_EMPTY, ICON_FLASH, 2),
    cv.Optional(CONF_GAIN_VOLTAGE, default=7305): cv.uint16_t,
    cv.Optional(CONF_GAIN_CT, default=27961): cv.uint16_t,
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ATM90E32Component),
    cv.Optional(CONF_PHASE_A): ATM90E32_PHASE_SCHEMA,
    cv.Optional(CONF_PHASE_B): ATM90E32_PHASE_SCHEMA,
    cv.Optional(CONF_PHASE_C): ATM90E32_PHASE_SCHEMA,
    cv.Optional(CONF_FREQUENCY): sensor.sensor_schema(UNIT_HERTZ, ICON_CURRENT_AC, 1),
    cv.Optional(CONF_CHIP_TEMPERATURE): sensor.sensor_schema(UNIT_CELSIUS, ICON_THERMOMETER, 1),
    cv.Required(CONF_LINE_FREQUENCY): cv.enum(LINE_FREQS, upper=True),
    cv.Optional(CONF_CURRENT_PHASES, default='3'): cv.enum(CURRENT_PHASES, upper=True),
    cv.Optional(CONF_GAIN_PGA, default='2X'): cv.enum(PGA_GAINS, upper=True),
}).extend(cv.polling_component_schema('60s')).extend(spi.spi_device_schema())


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield spi.register_spi_device(var, config)

    for i, phase in enumerate([CONF_PHASE_A, CONF_PHASE_B, CONF_PHASE_C]):
        if phase not in config:
            continue
        conf = config[phase]
        cg.add(var.set_volt_gain(i, conf[CONF_GAIN_VOLTAGE]))
        cg.add(var.set_ct_gain(i, conf[CONF_GAIN_CT]))
        if CONF_VOLTAGE in conf:
            sens = yield sensor.new_sensor(conf[CONF_VOLTAGE])
            cg.add(var.set_voltage_sensor(i, sens))
        if CONF_CURRENT in conf:
            sens = yield sensor.new_sensor(conf[CONF_CURRENT])
            cg.add(var.set_current_sensor(i, sens))
        if CONF_POWER in conf:
            sens = yield sensor.new_sensor(conf[CONF_POWER])
            cg.add(var.set_power_sensor(i, sens))
        if CONF_REACTIVE_POWER in conf:
            sens = yield sensor.new_sensor(conf[CONF_REACTIVE_POWER])
            cg.add(var.set_reactive_power_sensor(i, sens))
        if CONF_POWER_FACTOR in conf:
            sens = yield sensor.new_sensor(conf[CONF_POWER_FACTOR])
            cg.add(var.set_power_factor_sensor(i, sens))
    if CONF_FREQUENCY in config:
        sens = yield sensor.new_sensor(config[CONF_FREQUENCY])
        cg.add(var.set_freq_sensor(sens))
    if CONF_CHIP_TEMPERATURE in config:
        sens = yield sensor.new_sensor(config[CONF_CHIP_TEMPERATURE])
        cg.add(var.set_chip_temperature_sensor(sens))
    cg.add(var.set_line_freq(config[CONF_LINE_FREQUENCY]))
    cg.add(var.set_current_phases(config[CONF_CURRENT_PHASES]))
    cg.add(var.set_pga_gain(config[CONF_GAIN_PGA]))
