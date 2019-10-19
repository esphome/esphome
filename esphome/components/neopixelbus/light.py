import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light
from esphome.const import CONF_CLOCK_PIN, CONF_DATA_PIN, CONF_METHOD, CONF_NUM_LEDS, CONF_PIN, \
    CONF_TYPE, CONF_VARIANT, CONF_OUTPUT_ID
from esphome.core import CORE

neopixelbus_ns = cg.esphome_ns.namespace('neopixelbus')
NeoPixelBusLightOutputBase = neopixelbus_ns.class_('NeoPixelBusLightOutputBase',
                                                   light.AddressableLight)
NeoPixelRGBLightOutput = neopixelbus_ns.class_('NeoPixelRGBLightOutput', NeoPixelBusLightOutputBase)
NeoPixelRGBWLightOutput = neopixelbus_ns.class_('NeoPixelRGBWLightOutput',
                                                NeoPixelBusLightOutputBase)
ESPNeoPixelOrder = neopixelbus_ns.namespace('ESPNeoPixelOrder')
NeoRgbFeature = cg.global_ns.NeoRgbFeature
NeoRgbwFeature = cg.global_ns.NeoRgbwFeature


def validate_type(value):
    value = cv.string(value).upper()
    if 'R' not in value:
        raise cv.Invalid("Must have R in type")
    if 'G' not in value:
        raise cv.Invalid("Must have G in type")
    if 'B' not in value:
        raise cv.Invalid("Must have B in type")
    rest = set(value) - set('RGBW')
    if rest:
        raise cv.Invalid("Type has invalid color: {}".format(', '.join(rest)))
    if len(set(value)) != len(value):
        raise cv.Invalid("Type has duplicate color!")
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
            raise cv.Invalid("Method {} only supports pin(s) {}".format(
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
    'ESP32_RMT_0': 'NeoEsp32Rmt0{}Method',
    'ESP32_RMT_1': 'NeoEsp32Rmt1{}Method',
    'ESP32_RMT_2': 'NeoEsp32Rmt2{}Method',
    'ESP32_RMT_3': 'NeoEsp32Rmt3{}Method',
    'ESP32_RMT_4': 'NeoEsp32Rmt4{}Method',
    'ESP32_RMT_5': 'NeoEsp32Rmt5{}Method',
    'ESP32_RMT_6': 'NeoEsp32Rmt6{}Method',
    'ESP32_RMT_7': 'NeoEsp32Rmt7{}Method',
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
            raise cv.Invalid("Cannot specify both 'pin' and 'clock_pin'+'data_pin'")
        return config
    if CONF_CLOCK_PIN in config:
        if CONF_DATA_PIN not in config:
            raise cv.Invalid("If you give clock_pin, you must also specify data_pin")
        return config
    raise cv.Invalid("Must specify at least one of 'pin' or 'clock_pin'+'data_pin'")


CONFIG_SCHEMA = cv.All(light.ADDRESSABLE_LIGHT_SCHEMA.extend({
    cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(NeoPixelBusLightOutputBase),

    cv.Optional(CONF_TYPE, default='GRB'): validate_type,
    cv.Optional(CONF_VARIANT, default='800KBPS'): validate_variant,
    cv.Optional(CONF_METHOD, default=None): validate_method,
    cv.Optional(CONF_PIN): pins.output_pin,
    cv.Optional(CONF_CLOCK_PIN): pins.output_pin,
    cv.Optional(CONF_DATA_PIN): pins.output_pin,

    cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
}).extend(cv.COMPONENT_SCHEMA), validate, validate_method_pin)


def to_code(config):
    has_white = 'W' in config[CONF_TYPE]
    template = cg.TemplateArguments(getattr(cg.global_ns, format_method(config)))
    if has_white:
        out_type = NeoPixelRGBWLightOutput.template(template)
    else:
        out_type = NeoPixelRGBLightOutput.template(template)
    rhs = out_type.new()
    var = cg.Pvariable(config[CONF_OUTPUT_ID], rhs, type=out_type)
    yield light.register_light(var, config)
    yield cg.register_component(var, config)

    if CONF_PIN in config:
        cg.add(var.add_leds(config[CONF_NUM_LEDS], config[CONF_PIN]))
    else:
        cg.add(var.add_leds(config[CONF_NUM_LEDS], config[CONF_CLOCK_PIN], config[CONF_DATA_PIN]))

    cg.add(var.set_pixel_order(getattr(ESPNeoPixelOrder, config[CONF_TYPE])))

    # https://github.com/Makuna/NeoPixelBus/blob/master/library.json
    cg.add_library('NeoPixelBus-esphome', '2.5.2')
