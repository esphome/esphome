from esphome import pins
from esphome.components import display
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_DATA_PINS, CONF_DIMENSIONS, CONF_ENABLE_PIN, CONF_ID, \
    CONF_LAMBDA, CONF_RS_PIN, CONF_RW_PIN


LCDDisplay = display.display_ns.class_('LCDDisplay', PollingComponent)
LCDDisplayRef = LCDDisplay.operator('ref')
GPIOLCDDisplay = display.display_ns.class_('GPIOLCDDisplay', LCDDisplay)


def validate_lcd_dimensions(value):
    value = cv.dimensions(value)
    if value[0] > 0x40:
        raise cv.Invalid("LCD displays can't have more than 64 columns")
    if value[1] > 4:
        raise cv.Invalid("LCD displays can't have more than 4 rows")
    return value


def validate_pin_length(value):
    if len(value) != 4 and len(value) != 8:
        raise cv.Invalid("LCD Displays can either operate in 4-pin or 8-pin mode,"
                          "not {}-pin mode".format(len(value)))
    return value


PLATFORM_SCHEMA = display.BASIC_DISPLAY_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(GPIOLCDDisplay),
    cv.Required(CONF_DIMENSIONS): validate_lcd_dimensions,

    cv.Required(CONF_DATA_PINS): cv.All([pins.gpio_output_pin_schema], validate_pin_length),
    cv.Required(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
    cv.Required(CONF_RS_PIN): pins.gpio_output_pin_schema,
    cv.Optional(CONF_RW_PIN): pins.gpio_output_pin_schema,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    rhs = App.make_gpio_lcd_display(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1])
    lcd = Pvariable(config[CONF_ID], rhs)
    pins_ = []
    for conf in config[CONF_DATA_PINS]:
        pins_.append((yield gpio_output_pin_expression(conf)))
    cg.add(lcd.set_data_pins(*pins_))
    enable = yield gpio_output_pin_expression(config[CONF_ENABLE_PIN])
    cg.add(lcd.set_enable_pin(enable))

    rs = yield gpio_output_pin_expression(config[CONF_RS_PIN])
    cg.add(lcd.set_rs_pin(rs))

    if CONF_RW_PIN in config:
        rw = yield gpio_output_pin_expression(config[CONF_RW_PIN])
        cg.add(lcd.set_rw_pin(rw))

    if CONF_LAMBDA in config:
        lambda_ = yield process_lambda(config[CONF_LAMBDA], [(LCDDisplayRef, 'it')],
                                       return_type=void)
        cg.add(lcd.set_writer(lambda_))

    display.setup_display(lcd, config)
    register_component(lcd, config)


BUILD_FLAGS = '-DUSE_LCD_DISPLAY'
