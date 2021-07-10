from esphome import config_validation as cv
from esphome import codegen as cg
from esphome.const import CONF_BLUE, CONF_GREEN, CONF_ID, CONF_RED, CONF_WHITE

ColorStruct = cg.esphome_ns.struct("Color")

MULTI_CONF = True

CONF_RED_INT = "red_int"
CONF_GREEN_INT = "green_int"
CONF_BLUE_INT = "blue_int"
CONF_WHITE_INT = "white_int"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.declare_id(ColorStruct),
        cv.Exclusive(CONF_RED, "red"): cv.percentage,
        cv.Exclusive(CONF_RED_INT, "red"): cv.uint8_t,
        cv.Exclusive(CONF_GREEN, "green"): cv.percentage,
        cv.Exclusive(CONF_GREEN_INT, "green"): cv.uint8_t,
        cv.Exclusive(CONF_BLUE, "blue"): cv.percentage,
        cv.Exclusive(CONF_BLUE_INT, "blue"): cv.uint8_t,
        cv.Exclusive(CONF_WHITE, "white"): cv.percentage,
        cv.Exclusive(CONF_WHITE_INT, "white"): cv.uint8_t,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    r = 0
    if CONF_RED in config:
        r = int(config[CONF_RED] * 255)
    elif CONF_RED_INT in config:
        r = config[CONF_RED_INT]

    g = 0
    if CONF_GREEN in config:
        g = int(config[CONF_GREEN] * 255)
    elif CONF_GREEN_INT in config:
        g = config[CONF_GREEN_INT]

    b = 0
    if CONF_BLUE in config:
        b = int(config[CONF_BLUE] * 255)
    elif CONF_BLUE_INT in config:
        b = config[CONF_BLUE_INT]

    w = 0
    if CONF_WHITE in config:
        w = int(config[CONF_WHITE] * 255)
    elif CONF_WHITE_INT in config:
        w = config[CONF_WHITE_INT]

    cg.new_variable(
        config[CONF_ID],
        cg.StructInitializer(ColorStruct, ("r", r), ("g", g), ("b", b), ("w", w)),
    )
