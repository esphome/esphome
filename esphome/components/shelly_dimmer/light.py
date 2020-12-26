import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light, sensor
from esphome.const import CONF_OUTPUT_ID, CONF_GAMMA_CORRECT, \
    CONF_POWER, CONF_VOLTAGE, CONF_CURRENT, UNIT_VOLT, \
    ICON_FLASH, UNIT_AMPERE, UNIT_WATT

DEPENDENCIES = ['sensor']

shelly_ns = cg.esphome_ns.namespace('shelly')
ShellyDimmer = shelly_ns.class_('ShellyDimmer', light.LightOutput, cg.Component)

CONF_LEADING_EDGE = 'leading_edge'
CONF_WARMUP_BRIGHTNESS = 'warmup_brightness'
CONF_WARMUP_TIME = 'warmup_time'
CONF_MIN_BRIGHTNESS = 'min_brightness'
CONF_MAX_BRIGHTNESS = 'max_brightness'

CONF_NRST_PIN = 'nrst_pin'
CONF_BOOT0_PIN = 'boot0_pin'

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(ShellyDimmer),

    cv.Optional(CONF_NRST_PIN, default='GPIO5'): pins.gpio_output_pin_schema,
    cv.Optional(CONF_BOOT0_PIN, default='GPIO4'): pins.gpio_output_pin_schema,

    cv.Optional(CONF_LEADING_EDGE, default=False): cv.boolean,
    cv.Optional(CONF_WARMUP_BRIGHTNESS, default=100): cv.uint16_t,
    cv.Optional(CONF_WARMUP_TIME, default=20): cv.uint16_t,
    cv.Optional(CONF_MIN_BRIGHTNESS, default=0): cv.uint16_t,
    cv.Optional(CONF_MAX_BRIGHTNESS, default=1000): cv.uint16_t,

    cv.Optional(CONF_POWER): sensor.sensor_schema(UNIT_WATT, ICON_FLASH, 1),
    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(UNIT_VOLT, ICON_FLASH, 1),
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(UNIT_AMPERE, ICON_FLASH, 2),

    # Change the default gamma_correct setting.
    cv.Optional(CONF_GAMMA_CORRECT, default=1.0): cv.positive_float,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    yield cg.register_component(var, config)
    yield light.register_light(var, config)

    nrst_pin = yield cg.gpio_pin_expression(config[CONF_NRST_PIN])
    cg.add(var.set_nrst_pin(nrst_pin))
    boot0_pin = yield cg.gpio_pin_expression(config[CONF_BOOT0_PIN])
    cg.add(var.set_boot0_pin(boot0_pin))

    cg.add(var.set_leading_edge(config[CONF_LEADING_EDGE]))
    cg.add(var.set_warmup_brightness(config[CONF_WARMUP_BRIGHTNESS]))
    cg.add(var.set_warmup_time(config[CONF_WARMUP_TIME]))
    cg.add(var.set_min_brightness(config[CONF_MIN_BRIGHTNESS]))
    cg.add(var.set_max_brightness(config[CONF_MAX_BRIGHTNESS]))

    for key in [CONF_POWER, CONF_VOLTAGE, CONF_CURRENT]:
        if key not in config:
            continue

        conf = config[key]
        sens = yield sensor.new_sensor(conf)
        cg.add(getattr(var, f'set_{key}_sensor')(sens))
