import voluptuous as vol

from esphome import pins
from esphome.components import light
from esphome.components.light import AddressableLight
from esphome.components.power_supply import PowerSupplyComponent
import esphome.config_validation as cv
from esphome.const import CONF_CLOCK_PIN, CONF_COLOR_CORRECT, CONF_DATA_PIN, \
    CONF_DEFAULT_TRANSITION_LENGTH, CONF_EFFECTS, CONF_GAMMA_CORRECT, CONF_MAKE_ID, CONF_METHOD, \
    CONF_NAME, CONF_NUM_LEDS, CONF_PIN, CONF_POWER_SUPPLY, CONF_TYPE, CONF_VARIANT
from esphome.core import CORE
from esphome.cpp_generator import TemplateArguments, add, get_variable, variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, Application, Component, global_ns

NeoPixelBusLightOutputBase = light.light_ns.class_('NeoPixelBusLightOutputBase', Component,
                                                   AddressableLight)
ESPNeoPixelOrder = light.light_ns.namespace('ESPNeoPixelOrder')


def validate_type(value):
    value = cv.string(value).upper()
    if 'R' not in value:
        raise vol.Invalid("Must have R in type")
    if 'G' not in value:
        raise vol.Invalid("Must have G in type")
    if 'B' not in value:
        raise vol.Invalid("Must have B in type")
    rest = set(value) - set('RGBW')
    if rest:
        raise vol.Invalid("Type has invalid color: {}".format(', '.join(rest)))
    if len(set(value)) != len(value):
        raise vol.Invalid("Type has duplicate color!")
    return value


def validate_variant(value):
    value = cv.string(value).upper()
    if value == 'WS2813':
        value = 'WS2812X'
    if value == 'WS2812':
        value = '800KBPS'
    if value == 'LC8812':
        value = 'SK6812'
    return cv.one_of(*VARIANTS)(value)


def validate_method(value):
    if value is None:
        if CORE.is_esp32:
            return 'ESP32_I2S_1'
        if CORE.is_esp8266:
            return 'ESP8266_DMA'
        raise NotImplementedError

    if CORE.is_esp32:
        return cv.one_of(*ESP32_METHODS, upper=True, space='_')(value)
    if CORE.is_esp8266:
        return cv.one_of(*ESP8266_METHODS, upper=True, space='_')(value)
    raise NotImplementedError


def validate_method_pin(value):
    method = value[CONF_METHOD]
    method_pins = {
        'ESP8266_DMA': [3],
        'ESP8266_UART0': [1],
        'ESP8266_ASYNC_UART0': [1],
        'ESP8266_UART1': [2],
        'ESP8266_ASYNC_UART1': [2],
        'ESP32_I2S_0': list(range(0, 32)),
        'ESP32_I2S_1': list(range(0, 32)),
    }
    if CORE.is_esp8266:
        method_pins['BIT_BANG'] = list(range(0, 16))
    elif CORE.is_esp32:
        method_pins['BIT_BANG'] = list(range(0, 32))
    pins_ = method_pins[method]
    for opt in (CONF_PIN, CONF_CLOCK_PIN, CONF_DATA_PIN):
        if opt in value and value[opt] not in pins_:
            raise vol.Invalid("Method {} only supports pin(s) {}".format(
                method, ', '.join('GPIO{}'.format(x) for x in pins_)
            ), path=[CONF_METHOD])
    return value


VARIANTS = {
    'WS2812X': 'Ws2812x',
    'SK6812': 'Sk6812',
    '800KBPS': '800Kbps',
    '400KBPS': '400Kbps',
}

ESP8266_METHODS = {
    'ESP8266_DMA': 'NeoEsp8266Dma{}Method',
    'ESP8266_UART0': 'NeoEsp8266Uart0{}Method',
    'ESP8266_UART1': 'NeoEsp8266Uart1{}Method',
    'ESP8266_ASYNC_UART0': 'NeoEsp8266AsyncUart0{}Method',
    'ESP8266_ASYNC_UART1': 'NeoEsp8266AsyncUart1{}Method',
    'BIT_BANG': 'NeoEsp8266BitBang{}Method',
}
ESP32_METHODS = {
    'ESP32_I2S_0': 'NeoEsp32I2s0{}Method',
    'ESP32_I2S_1': 'NeoEsp32I2s1{}Method',
    'BIT_BANG': 'NeoEsp32BitBang{}Method',
}


def format_method(config):
    variant = VARIANTS[config[CONF_VARIANT]]
    method = config[CONF_METHOD]
    if CORE.is_esp8266:
        return ESP8266_METHODS[method].format(variant)
    if CORE.is_esp32:
        return ESP32_METHODS[method].format(variant)
    raise NotImplementedError


def validate(config):
    if CONF_PIN in config:
        if CONF_CLOCK_PIN in config or CONF_DATA_PIN in config:
            raise vol.Invalid("Cannot specify both 'pin' and 'clock_pin'+'data_pin'")
        return config
    if CONF_CLOCK_PIN in config:
        if CONF_DATA_PIN not in config:
            raise vol.Invalid("If you give clock_pin, you must also specify data_pin")
        return config
    raise vol.Invalid("Must specify at least one of 'pin' or 'clock_pin'+'data_pin'")


MakeNeoPixelBusLight = Application.struct('MakeNeoPixelBusLight')

PLATFORM_SCHEMA = cv.nameable(light.LIGHT_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(light.AddressableLightState),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeNeoPixelBusLight),

    vol.Optional(CONF_TYPE, default='GRB'): validate_type,
    vol.Optional(CONF_VARIANT, default='800KBPS'): validate_variant,
    vol.Optional(CONF_METHOD, default=None): validate_method,
    vol.Optional(CONF_PIN): pins.output_pin,
    vol.Optional(CONF_CLOCK_PIN): pins.output_pin,
    vol.Optional(CONF_DATA_PIN): pins.output_pin,

    vol.Required(CONF_NUM_LEDS): cv.positive_not_null_int,

    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_COLOR_CORRECT): vol.All([cv.percentage], vol.Length(min=3, max=4)),
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_POWER_SUPPLY): cv.use_variable_id(PowerSupplyComponent),
    vol.Optional(CONF_EFFECTS): light.validate_effects(light.ADDRESSABLE_EFFECTS),
}).extend(cv.COMPONENT_SCHEMA.schema), validate, validate_method_pin)


def to_code(config):
    type_ = config[CONF_TYPE]
    has_white = 'W' in type_
    if has_white:
        func = App.make_neo_pixel_bus_rgbw_light
        color_feat = global_ns.NeoRgbwFeature
    else:
        func = App.make_neo_pixel_bus_rgb_light
        color_feat = global_ns.NeoRgbFeature

    template = TemplateArguments(getattr(global_ns, format_method(config)), color_feat)
    rhs = func(template, config[CONF_NAME])
    make = variable(config[CONF_MAKE_ID], rhs, type=MakeNeoPixelBusLight.template(template))
    output = make.Poutput

    if CONF_PIN in config:
        add(output.add_leds(config[CONF_NUM_LEDS], config[CONF_PIN]))
    else:
        add(output.add_leds(config[CONF_NUM_LEDS], config[CONF_CLOCK_PIN], config[CONF_DATA_PIN]))

    add(output.set_pixel_order(getattr(ESPNeoPixelOrder, type_)))

    if CONF_POWER_SUPPLY in config:
        for power_supply in get_variable(config[CONF_POWER_SUPPLY]):
            yield
        add(output.set_power_supply(power_supply))

    if CONF_COLOR_CORRECT in config:
        add(output.set_correction(*config[CONF_COLOR_CORRECT]))

    light.setup_light(make.Pstate, config)
    setup_component(output, config)


REQUIRED_BUILD_FLAGS = '-DUSE_NEO_PIXEL_BUS_LIGHT'

LIB_DEPS = 'NeoPixelBus@2.4.1'


def to_hass_config(data, config):
    return light.core_to_hass_config(data, config, brightness=True, rgb=True, color_temp=False,
                                     white_value='W' in config[CONF_TYPE])
