import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import CONF_WIDTH, CONF_HEIGHT, \
    CONF_ID, CONF_LAMBDA, CONF_PAGES

pxmatrix_ns = cg.esphome_ns.namespace('pxmatrix_display')

pxmatrix_gpio = pxmatrix_ns.class_('PxmatrixDisplay', cg.PollingComponent, display.DisplayBuffer)

DRIVER_CHIPS = {"SHIFT", "FM6124", "FM6126A"}

COLOR_ORDERS = {"RRGGBB", "RRBBGG", "GGRRBB", "GGBBRR", "BBRRGG", "BBGGRR"}

BLOCK_PATTERNS = {"ABCD", "DBCA"}

MUX_PATTERNS = {"BINARY", "STRAIGHT", "SHIFTREG_ABC", "SHIFTREG_SPI_SE", "SHIFTREG_ABC_BIN_DE"}

SCAN_PATTERNS = {"LINE", "ZIGZAG", "ZZAGG", "ZAGGIZ", "WZAGZIG", "VZAG", "ZAGZIG", "WZAGZIG2", "ZZIAGG"}

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend({
        cv.GenerateID(): cv.declare_id(pxmatrix_gpio),
        cv.Optional(CONF_WIDTH): cv.uint8_t,
        cv.Optional(CONF_HEIGHT): cv.uint8_t,
        cv.Optional("display_draw_time"): cv.uint8_t,
        cv.Optional("p_lat"): cv.uint8_t,
        cv.Optional('p_a'): cv.uint8_t,
        cv.Optional('p_b'): cv.uint8_t,
        cv.Optional('p_c'): cv.uint8_t,
        cv.Optional('p_d'): cv.uint8_t,
        cv.Optional('p_e'): cv.uint8_t,
        cv.Optional('p_oe'): cv.uint8_t,
        cv.Optional("driver_chip"): cv.one_of(*DRIVER_CHIPS),
        cv.Optional("color_order"): cv.one_of(*COLOR_ORDERS),
        cv.Optional("block_pattern"): cv.one_of(*BLOCK_PATTERNS),
        cv.Optional("mux_pattern"): cv.one_of(*MUX_PATTERNS),
        cv.Optional("scan_pattern"): cv.one_of(*SCAN_PATTERNS),
    }).extend(cv.polling_component_schema('1ms')),
    cv.has_at_most_one_key(CONF_PAGES, CONF_LAMBDA))

def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    yield cg.register_component(var, config)
    yield display.register_display(var, config)

    if CONF_LAMBDA in config:
        lambda_ = yield cg.process_lambda(
            config[CONF_LAMBDA],
            [(display.DisplayBufferRef, 'it')],
            return_type=cg.void)
        cg.add(var.set_writer(lambda_))
