import voluptuous as vol

from esphome import pins
from esphome.components import light
from esphome.components.power_supply import PowerSupplyComponent
import esphome.config_validation as cv
from esphome.const import CONF_CHIPSET, CONF_CLOCK_PIN, CONF_DATA_PIN, CONF_EFFECTS, CONF_MAKE_ID, \
    CONF_MAX_REFRESH_RATE, CONF_NAME, CONF_NUM_LEDS, CONF_POWER_SUPPLY, CONF_RGB_ORDER
from esphome.cpp_generator import RawExpression, TemplateArguments, add, get_variable, variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Application

CHIPSETS = [
    'LPD8806',
    'WS2801',
    'WS2803',
    'SM16716',
    'P9813',
    'APA102',
    'SK9822',
    'DOTSTAR',
]

RGB_ORDERS = [
    'RGB',
    'RBG',
    'GRB',
    'GBR',
    'BRG',
    'BGR',
]

MakeFastLEDLight = Application.struct('MakeFastLEDLight')

PLATFORM_SCHEMA = cv.nameable(light.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeFastLEDLight),

    vol.Required(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
    vol.Required(CONF_DATA_PIN): pins.output_pin,
    vol.Required(CONF_CLOCK_PIN): pins.output_pin,

    vol.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
    vol.Optional(CONF_RGB_ORDER): cv.one_of(*RGB_ORDERS, upper=True),
    vol.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,

    vol.Optional(CONF_POWER_SUPPLY): cv.use_variable_id(PowerSupplyComponent),
    vol.Optional(CONF_EFFECTS): light.validate_effects(
        light.ADDRESSABLE_EFFECTS),
}).extend(light.ADDRESSABLE_LIGHT_SCHEMA.schema).extend(
    cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_fast_led_light(config[CONF_NAME])
    make = variable(config[CONF_MAKE_ID], rhs)
    fast_led = make.Pfast_led

    rgb_order = None
    if CONF_RGB_ORDER in config:
        rgb_order = RawExpression(config[CONF_RGB_ORDER])
    template_args = TemplateArguments(RawExpression(config[CONF_CHIPSET]),
                                      config[CONF_DATA_PIN],
                                      config[CONF_CLOCK_PIN],
                                      rgb_order)
    add(fast_led.add_leds(template_args, config[CONF_NUM_LEDS]))

    if CONF_MAX_REFRESH_RATE in config:
        add(fast_led.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    if CONF_POWER_SUPPLY in config:
        for power_supply in get_variable(config[CONF_POWER_SUPPLY]):
            yield
        add(fast_led.set_power_supply(power_supply))

    light.setup_light(make.Pstate, make.Pfast_led, config)
    setup_component(fast_led, config)


REQUIRED_BUILD_FLAGS = '-DUSE_FAST_LED_LIGHT'

LIB_DEPS = 'FastLED@3.2.0'
