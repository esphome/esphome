import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import light
from esphomeyaml.components.power_supply import PowerSupplyComponent
from esphomeyaml.const import CONF_CHIPSET, CONF_CLOCK_PIN, CONF_DATA_PIN, \
    CONF_DEFAULT_TRANSITION_LENGTH, CONF_GAMMA_CORRECT, CONF_MAKE_ID, CONF_MAX_REFRESH_RATE, \
    CONF_NAME, CONF_NUM_LEDS, CONF_POWER_SUPPLY, CONF_RGB_ORDER, CONF_EFFECTS
from esphomeyaml.helpers import App, Application, RawExpression, TemplateArguments, add, \
    get_variable, variable

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

MakeFastLEDLight = Application.MakeFastLEDLight

PLATFORM_SCHEMA = cv.nameable(light.LIGHT_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeFastLEDLight),

    vol.Required(CONF_CHIPSET): vol.All(vol.Upper, cv.one_of(*CHIPSETS)),
    vol.Required(CONF_DATA_PIN): pins.output_pin,
    vol.Required(CONF_CLOCK_PIN): pins.output_pin,

    vol.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
    vol.Optional(CONF_RGB_ORDER): vol.All(vol.Upper, cv.one_of(*RGB_ORDERS)),
    vol.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,

    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_POWER_SUPPLY): cv.use_variable_id(PowerSupplyComponent),
    vol.Optional(CONF_EFFECTS): light.validate_effects(light.FASTLED_EFFECTS),
}))


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
        power_supply = None
        for power_supply in get_variable(config[CONF_POWER_SUPPLY]):
            yield
        add(fast_led.set_power_supply(power_supply))

        light.setup_light(make.Pstate, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_FAST_LED_LIGHT'
