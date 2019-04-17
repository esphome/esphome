import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor
from esphome.const import CONF_ATTENUATION, CONF_ID, CONF_NAME, CONF_PIN, CONF_UPDATE_INTERVAL, \
    CONF_ICON, ICON_FLASH, CONF_UNIT_OF_MEASUREMENT, UNIT_VOLT, CONF_ACCURACY_DECIMALS

ATTENUATION_MODES = {
    '0db': cg.global_ns.ADC_0db,
    '2.5db': cg.global_ns.ADC_2_5db,
    '6db': cg.global_ns.ADC_6db,
    '11db': cg.global_ns.ADC_11db,
}


def validate_adc_pin(value):
    vcc = str(value).upper()
    if vcc == 'VCC':
        return cv.only_on_esp8266(vcc)
    return pins.analog_pin(value)


adc_ns = cg.esphome_ns.namespace('adc')
ADCSensor = adc_ns.class_('ADCSensor', sensor.PollingSensorComponent)

CONFIG_SCHEMA = cv.nameable(sensor.SENSOR_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(ADCSensor),
    cv.Required(CONF_PIN): validate_adc_pin,
    cv.Optional(CONF_ATTENUATION): cv.All(cv.only_on_esp32, cv.one_of(*ATTENUATION_MODES,
                                                                      lower=True)),

    cv.Optional(CONF_UPDATE_INTERVAL, default='60s'): cv.update_interval,
    cv.Optional(CONF_ICON, default=ICON_FLASH): sensor.icon,
    cv.Optional(CONF_UNIT_OF_MEASUREMENT, default=UNIT_VOLT): sensor.unit_of_measurement,
    cv.Optional(CONF_ACCURACY_DECIMALS, default=2): sensor.accuracy_decimals,
}).extend(cv.COMPONENT_SCHEMA))


def to_code(config):
    pin = config[CONF_PIN]
    if pin == 'VCC':
        pin = 0

        cg.add_define('USE_ADC_SENSOR_VCC')
        cg.add_global(cg.global_ns.ADC_MODE(cg.global_ns.ADC_VCC))
    rhs = ADCSensor.new(config[CONF_NAME], pin, config[CONF_UPDATE_INTERVAL])
    adc = cg.Pvariable(config[CONF_ID], rhs)
    yield cg.register_component(adc, config)
    yield sensor.register_sensor(adc, config)

    if CONF_ATTENUATION in config:
        cg.add(adc.set_attenuation(ATTENUATION_MODES[config[CONF_ATTENUATION]]))
