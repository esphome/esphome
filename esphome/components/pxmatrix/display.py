import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import CONF_WIDTH, CONF_HEIGHT, \
    CONF_ID, CONF_LAMBDA, CONF_PAGES, CONF_BRIGHTNESS, CONF_RGB_ORDER, \
    CONF_PIN_A, CONF_PIN_B, CONF_PIN_C, CONF_PIN_D, CONF_PIN_E, CONF_PIN_OE, CONF_PIN_LATCH, \
    CONF_CHIPSET, CONF_MULTIPLEXER, CONF_SCAN_PATTERN, CONF_ROW_PATTERN

pxmatrix_ns = cg.esphome_ns.namespace('pxmatrix_display')

pxmatrix_gpio = pxmatrix_ns.class_('PxmatrixDisplay', cg.PollingComponent, display.DisplayBuffer)

DriverChips = pxmatrix_ns.enum('DriverChips')
DRIVER_CHIPS = {
    'SHIFT': DriverChips.SHIFT,
    "FM6124": DriverChips.FM6124,
    "FM6126A": DriverChips.FM6126A
}
Color_Orders = pxmatrix_ns.enum('ColorOrders')
COLOR_ORDERS = {
    "RRGGBB": Color_Orders.RRGGBB,
    "RRBBGG": Color_Orders.RRBBGG,
    "GGRRBB": Color_Orders.GGRRBB,
    "GGBBRR": Color_Orders.GGBBRR,
    "BBRRGG": Color_Orders.BBRRGG,
    "BBGGRR": Color_Orders.BBGGRR,
}
ScanPatterns = pxmatrix_ns.enum('ScanPatterns')
SCAN_PATTERNS = {
    "LINE": ScanPatterns.LINE,
    "ZIGZAG": ScanPatterns.ZIGZAG,
    "VZAG": ScanPatterns.VZAG,
    "WZAGZIG": ScanPatterns.WZAGZIG,
    "ZAGGIZ": ScanPatterns.ZAGGIZ,
    "ZZAGG": ScanPatterns.ZZAGG,
}

MuxPatterns = pxmatrix_ns.enum('MuxPatterns')
MUX_PATTERNS = {
    "BINARY": MuxPatterns.BINARY,
    "STRAIGHT": MuxPatterns.STRAIGHT,
}

# Block Pattern is not Compiling for some strange reason.
# BLOCK_PATTERNS = {"ABCD", "DBCA"}


ROW_PATTERNS = {64, 32, 16, 8, 4, 2}
CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(pxmatrix_gpio),
        cv.Optional(CONF_WIDTH, default="32"): cv.uint8_t,
        cv.Optional(CONF_HEIGHT, default="32"): cv.uint8_t,
        cv.Optional(CONF_BRIGHTNESS, default="255"): cv.uint8_t,
        cv.Optional(CONF_PIN_LATCH, default="GPIO16"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_PIN_A, default="GPIO5"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_PIN_B, default="GPIO4"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_PIN_C, default="GPIO15"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_PIN_D, default="GPIO12"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_PIN_E, default="GPIO0"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_PIN_OE, default="GPIO2"): pins.gpio_output_pin_schema,
        cv.Optional(CONF_ROW_PATTERN, default=32): cv.one_of(*ROW_PATTERNS),
        cv.Optional(CONF_CHIPSET, default="FM6124"): cv.enum(DRIVER_CHIPS),
        cv.Optional(CONF_RGB_ORDER, default="RRGGBB"): cv.enum(COLOR_ORDERS),
        cv.Optional(CONF_MULTIPLEXER, default="BINARY"): cv.enum(MUX_PATTERNS),
        cv.Optional(CONF_SCAN_PATTERN, default="LINE"): cv.enum(SCAN_PATTERNS),
        # cv.Optional("block_pattern"): cv.one_of(*BLOCK_PATTERNS),
    }),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    yield cg.register_component(var, config)
    yield display.register_display(var, config)

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(config[CONF_LAMBDA], [(display.DisplayBufferRef, 'it')],
                                          return_type=cg.void)
        cg.add(var.set_writer(lambda_))

    cg.add(var.set_color_orders(config[CONF_RGB_ORDER]))
    cg.add(var.set_mux_patterns(config[CONF_MULTIPLEXER]))
    cg.add(var.set_scan_patterns(config[CONF_SCAN_PATTERN]))
    cg.add(var.set_driver_chips(config[CONF_CHIPSET]))

    if CONF_PIN_LATCH in config:
        latch = yield cg.gpio_pin_expression(config[CONF_PIN_LATCH])
        cg.add(var.set_pin_latch(latch))

    if CONF_PIN_A in config:
        latch = yield cg.gpio_pin_expression(config[CONF_PIN_A])
        cg.add(var.set_pin_a(latch))

    if CONF_PIN_B in config:
        latch = yield cg.gpio_pin_expression(config[CONF_PIN_B])
        cg.add(var.set_pin_b(latch))

    if CONF_PIN_C in config:
        latch = yield cg.gpio_pin_expression(config[CONF_PIN_C])
        cg.add(var.set_pin_c(latch))

    if CONF_PIN_D in config:
        latch = yield cg.gpio_pin_expression(config[CONF_PIN_D])
        cg.add(var.set_pin_d(latch))

    if CONF_PIN_E in config:
        latch = yield cg.gpio_pin_expression(config[CONF_PIN_E])
        cg.add(var.set_pin_e(latch))

    if CONF_PIN_OE in config:
        latch = yield cg.gpio_pin_expression(config[CONF_PIN_OE])
        cg.add(var.set_pin_oe(latch))

    if CONF_WIDTH in config:
        cg.add(var.set_width(config[CONF_WIDTH]))

    if CONF_HEIGHT in config:
        cg.add(var.set_height(config[CONF_HEIGHT]))

    if CONF_BRIGHTNESS in config:
        cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))

    if CONF_ROW_PATTERN in config:
        cg.add(var.set_row_patter(config[CONF_ROW_PATTERN]))

    # https://github.com/2dom/PxMatrix/blob/master/PxMatrix.h
    cg.add_library("PxMatrix LED MATRIX library", "1.8.2")
    # Adafruit GF https://github.com/adafruit/Adafruit-GFX-Library/releases
    cg.add_library("13", "1.10.2")
    cg.add_library("Wire", "1.0")



