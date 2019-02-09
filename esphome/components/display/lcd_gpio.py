import voluptuous as vol

from esphome import pins
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_DATA_PINS, CONF_DIMENSIONS, CONF_ENABLE_PIN, CONF_ID, \
    CONF_LAMBDA, CONF_RS_PIN, CONF_RW_PIN
from esphome.cpp_generator import Pvariable, add, process_lambda
from esphome.cpp_helpers import gpio_output_pin_expression, setup_component
from esphome.cpp_types import App, PollingComponent, void

LCDDisplay = display.display_ns.class_('LCDDisplay', PollingComponent)
LCDDisplayRef = LCDDisplay.operator('ref')
GPIOLCDDisplay = display.display_ns.class_('GPIOLCDDisplay', LCDDisplay)


def validate_lcd_dimensions(value):
    value = cv.dimensions(value)
    if value[0] > 0x40:
        raise vol.Invalid("LCD displays can't have more than 64 columns")
    if value[1] > 4:
        raise vol.Invalid("LCD displays can't have more than 4 rows")
    return value


def validate_pin_length(value):
    if len(value) != 4 and len(value) != 8:
        raise vol.Invalid("LCD Displays can either operate in 4-pin or 8-pin mode,"
                          "not {}-pin mode".format(len(value)))
    return value


PLATFORM_SCHEMA = display.BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GPIOLCDDisplay),
    vol.Required(CONF_DIMENSIONS): validate_lcd_dimensions,

    vol.Required(CONF_DATA_PINS): vol.All([pins.gpio_output_pin_schema], validate_pin_length),
    vol.Required(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_RS_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_RW_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    rhs = App.make_gpio_lcd_display(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1])
    lcd = Pvariable(config[CONF_ID], rhs)
    pins_ = []
    for conf in config[CONF_DATA_PINS]:
        for pin in gpio_output_pin_expression(conf):
            yield
        pins_.append(pin)
    add(lcd.set_data_pins(*pins_))
    for enable in gpio_output_pin_expression(config[CONF_ENABLE_PIN]):
        yield
    add(lcd.set_enable_pin(enable))

    for rs in gpio_output_pin_expression(config[CONF_RS_PIN]):
        yield
    add(lcd.set_rs_pin(rs))

    if CONF_RW_PIN in config:
        for rw in gpio_output_pin_expression(config[CONF_RW_PIN]):
            yield
        add(lcd.set_rw_pin(rw))

    if CONF_LAMBDA in config:
        for lambda_ in process_lambda(config[CONF_LAMBDA], [(LCDDisplayRef, 'it')],
                                      return_type=void):
            yield
        add(lcd.set_writer(lambda_))

    display.setup_display(lcd, config)
    setup_component(lcd, config)


BUILD_FLAGS = '-DUSE_LCD_DISPLAY'
