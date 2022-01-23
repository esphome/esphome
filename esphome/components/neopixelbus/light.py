import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light
from esphome.const import (
    CONF_CHANNEL,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_METHOD,
    CONF_NUM_LEDS,
    CONF_PIN,
    CONF_TYPE,
    CONF_VARIANT,
    CONF_OUTPUT_ID,
    CONF_INVERT,
)
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32C3,
)
from esphome.core import CORE
from ._methods import (
    METHODS,
    METHOD_SPI,
    METHOD_ESP8266_UART,
    METHOD_BIT_BANG,
    METHOD_ESP32_I2S,
    METHOD_ESP32_RMT,
    METHOD_ESP8266_DMA,
)
from .const import (
    CHIP_TYPES,
    CONF_ASYNC,
    CONF_BUS,
    ONE_WIRE_CHIPS,
)

neopixelbus_ns = cg.esphome_ns.namespace("neopixelbus")
NeoPixelBusLightOutputBase = neopixelbus_ns.class_(
    "NeoPixelBusLightOutputBase", light.AddressableLight
)
NeoPixelRGBLightOutput = neopixelbus_ns.class_(
    "NeoPixelRGBLightOutput", NeoPixelBusLightOutputBase
)
NeoPixelRGBWLightOutput = neopixelbus_ns.class_(
    "NeoPixelRGBWLightOutput", NeoPixelBusLightOutputBase
)
ESPNeoPixelOrder = neopixelbus_ns.namespace("ESPNeoPixelOrder")
NeoRgbFeature = cg.global_ns.NeoRgbFeature
NeoRgbwFeature = cg.global_ns.NeoRgbwFeature


def validate_type(value):
    value = cv.string(value).upper()
    if "R" not in value:
        raise cv.Invalid("Must have R in type")
    if "G" not in value:
        raise cv.Invalid("Must have G in type")
    if "B" not in value:
        raise cv.Invalid("Must have B in type")
    rest = set(value) - set("RGBW")
    if rest:
        raise cv.Invalid(f"Type has invalid color: {', '.join(rest)}")
    if len(set(value)) != len(value):
        raise cv.Invalid("Type has duplicate color!")
    return value


def _choose_default_method(config):
    if CONF_METHOD in config:
        return config
    config = config.copy()
    if CONF_PIN not in config:
        config[CONF_METHOD] = _validate_method(METHOD_SPI)
        return config

    pin = config[CONF_PIN]
    if CORE.is_esp8266:
        if pin == 3:
            config[CONF_METHOD] = _validate_method(METHOD_ESP8266_DMA)
        elif pin == 1:
            config[CONF_METHOD] = _validate_method(
                {
                    CONF_TYPE: METHOD_ESP8266_UART,
                    CONF_BUS: 0,
                }
            )
        elif pin == 2:
            config[CONF_METHOD] = _validate_method(
                {
                    CONF_TYPE: METHOD_ESP8266_UART,
                    CONF_BUS: 1,
                }
            )
        else:
            config[CONF_METHOD] = _validate_method(METHOD_BIT_BANG)

    if CORE.is_esp32:
        if get_esp32_variant() == VARIANT_ESP32C3:
            config[CONF_METHOD] = _validate_method(METHOD_ESP32_RMT)
        else:
            config[CONF_METHOD] = _validate_method(METHOD_ESP32_I2S)

    return config


def _validate(config):
    variant = config[CONF_VARIANT]
    if variant in ONE_WIRE_CHIPS:
        if CONF_PIN not in config:
            raise cv.Invalid(
                f"Chip {variant} is a 1-wire chip and needs the [pin] option."
            )
        if CONF_CLOCK_PIN in config or CONF_DATA_PIN in config:
            raise cv.Invalid(
                f"Chip {variant} is a 1-wire chip, you need to set [pin] instead of ."
            )
    else:
        if CONF_PIN in config:
            raise cv.Invalid(
                f"Chip {variant} is a 2-wire chip and needs the [data_pin]+[clock_pin] option instead of [pin]."
            )
        if CONF_CLOCK_PIN not in config or CONF_DATA_PIN not in config:
            raise cv.Invalid(
                f"Chip {variant} is a 2-wire chip, you need to set [data_pin]+[clock_pin]."
            )

    method_type = config[CONF_METHOD][CONF_TYPE]
    method_desc = METHODS[method_type]
    if variant not in method_desc.supported_chips:
        raise cv.Invalid(f"Method {method_type} does not support {variant}")
    if method_desc.extra_validate is not None:
        method_desc.extra_validate(config)

    return config


def _validate_method(value):
    if value is None:
        # default method is determined afterwards because it depends on the chip type chosen
        return None

    compat_methods = {}
    for bus in [0, 1]:
        for is_async in [False, True]:
            compat_methods[f"ESP8266{'_ASYNC' if is_async else ''}_UART{bus}"] = {
                CONF_TYPE: METHOD_ESP8266_UART,
                CONF_BUS: bus,
                CONF_ASYNC: is_async,
            }
        compat_methods[f"ESP32_I2S_{bus}"] = {
            CONF_TYPE: METHOD_ESP32_I2S,
            CONF_BUS: bus,
        }
    for channel in range(8):
        compat_methods[f"ESP32_RMT_{channel}"] = {
            CONF_TYPE: METHOD_ESP32_RMT,
            CONF_CHANNEL: channel,
        }

    if isinstance(value, str):
        if value.upper() in compat_methods:
            return _validate_method(compat_methods[value.upper()])
        return _validate_method({CONF_TYPE: value})
    return cv.typed_schema(
        {k: v.method_schema for k, v in METHODS.items()}, lower=True
    )(value)


CONFIG_SCHEMA = cv.All(
    cv.only_with_arduino,
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 4, 0),
        esp32_arduino=cv.Version(0, 0, 0),
    ),
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(NeoPixelBusLightOutputBase),
            cv.Optional(CONF_TYPE, default="GRB"): validate_type,
            cv.Required(CONF_VARIANT): cv.one_of(*CHIP_TYPES, lower=True),
            cv.Optional(CONF_METHOD): _validate_method,
            cv.Optional(CONF_INVERT, default="no"): cv.boolean,
            cv.Optional(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_CLOCK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_DATA_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _choose_default_method,
    _validate,
)


async def to_code(config):
    has_white = "W" in config[CONF_TYPE]
    method = config[CONF_METHOD]

    method_template = METHODS[method[CONF_TYPE]].to_code(
        method, config[CONF_VARIANT], config[CONF_INVERT]
    )

    if has_white:
        out_type = NeoPixelRGBWLightOutput.template(method_template)
    else:
        out_type = NeoPixelRGBLightOutput.template(method_template)
    rhs = out_type.new()
    var = cg.Pvariable(config[CONF_OUTPUT_ID], rhs, out_type)
    await light.register_light(var, config)
    await cg.register_component(var, config)

    if CONF_PIN in config:
        cg.add(var.add_leds(config[CONF_NUM_LEDS], config[CONF_PIN]))
    else:
        cg.add(
            var.add_leds(
                config[CONF_NUM_LEDS], config[CONF_CLOCK_PIN], config[CONF_DATA_PIN]
            )
        )

    cg.add(var.set_pixel_order(getattr(ESPNeoPixelOrder, config[CONF_TYPE])))

    # https://github.com/Makuna/NeoPixelBus/blob/master/library.json
    cg.add_library("makuna/NeoPixelBus", "2.6.9")
