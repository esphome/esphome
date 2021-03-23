import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import core, automation
from esphome.components.waveshare_epaper.waveshare_enum import WAVESHARE_COLORS_ENUM
from esphome.automation import maybe_simple_id
from esphome.const import (
    CONF_ID,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_ROTATION,
    CONF_TYPE,
    CONF_COLORS,
)
from esphome.core import coroutine, coroutine_with_priority
from esphome.components import color

CONF_BUFFER = "buffer"
CONF_COLOR_OFF = "color_off"
CONF_COLOR_ON = "color_on"
CONF_BUFFER_ID = "buffer_id"
CONF_INDEX_SIZE = "index_size"
CONF_COLOR = "color"
CONF_WAVESHARE_COLORS = "waveshare_colors"
CONF_BUFFER_INDEX_SIZE = "buffer_index_size"
CONF_DEVICE_WIDTH = "device_width"
CONF_DEVICE_HEIGHT = "device_height"
CONF_ROW_START = "row_start"
CONF_COL_START = "col_start"

IS_PLATFORM_COMPONENT = True

display_ns = cg.esphome_ns.namespace("display")
DisplayBuffer = display_ns.class_("DisplayBuffer")
DisplayPage = display_ns.class_("DisplayPage")
DisplayPagePtr = DisplayPage.operator("ptr")
DisplayBufferRef = DisplayBuffer.operator("ref")
DisplayPageShowAction = display_ns.class_("DisplayPageShowAction", automation.Action)
DisplayPageShowNextAction = display_ns.class_(
    "DisplayPageShowNextAction", automation.Action
)
DisplayPageShowPrevAction = display_ns.class_(
    "DisplayPageShowPrevAction", automation.Action
)

buffer_base = display_ns.class_("BufferBase")
buffer_565 = display_ns.class_("Buffer565")
buffer_666 = display_ns.class_("Buffer666")
buffer_332 = display_ns.class_("Buffer332")
buffer_indexed8 = display_ns.class_("BufferIndexed8")

BufferType = display_ns.enum("BufferType")

TYPES = {
    "RGB666": BufferType.RGB666,
    "RGB565": BufferType.RGB565,
    "RGB332": BufferType.RGB332,
    "INDEXED8": BufferType.INDEXED8,
}
BUFFER_TYPES = cv.enum(TYPES, upper=True, space="_")

BASIC_BUFFER_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TYPE): BUFFER_TYPES,
        cv.Optional(CONF_COLOR_ON): cv.use_id(color),
        cv.Optional(CONF_COLOR_OFF): cv.use_id(color),
        cv.Optional(CONF_COLORS): cv.ensure_list(
            {
                cv.Required(CONF_COLOR): cv.use_id(color),
            }
        ),
        cv.Optional(CONF_INDEX_SIZE): cv.int_range(min=1, max=255),
    }
)

DISPLAY_ROTATIONS = {
    0: display_ns.DISPLAY_ROTATION_0_DEGREES,
    90: display_ns.DISPLAY_ROTATION_90_DEGREES,
    180: display_ns.DISPLAY_ROTATION_180_DEGREES,
    270: display_ns.DISPLAY_ROTATION_270_DEGREES,
}


def validate_rotation(value):
    value = cv.string(value)
    if value.endswith("°"):
        value = value[:-1]
    return cv.enum(DISPLAY_ROTATIONS, int=True)(value)


BASIC_DISPLAY_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_LAMBDA): cv.lambda_,
    }
)

FULL_DISPLAY_SCHEMA = BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(CONF_BUFFER_ID): cv.declare_id(buffer_base),
        cv.Optional(CONF_ROTATION): validate_rotation,
        cv.Optional(CONF_DEVICE_WIDTH): cv.positive_int,
        cv.Optional(CONF_DEVICE_HEIGHT): cv.positive_int,
        cv.Optional(CONF_COL_START): cv.positive_int,
        cv.Optional(CONF_ROW_START): cv.positive_int,
        cv.Optional(CONF_BUFFER): cv.All(BASIC_BUFFER_SCHEMA),
        cv.Optional(CONF_PAGES): cv.All(
            cv.ensure_list(
                {
                    cv.GenerateID(): cv.declare_id(DisplayPage),
                    cv.Required(CONF_LAMBDA): cv.lambda_,
                }
            ),
            cv.Length(min=1),
        ),
    }
)

FULL_DISPLAY_NO_BUFFER_SCHEMA = BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(CONF_BUFFER_ID): cv.declare_id(buffer_base),
        cv.Optional(CONF_ROTATION): validate_rotation,
        cv.Optional(CONF_PAGES): cv.All(
            cv.ensure_list(
                {
                    cv.GenerateID(): cv.declare_id(DisplayPage),
                    cv.Required(CONF_LAMBDA): cv.lambda_,
                }
            ),
            cv.Length(min=1),
        ),
    }
)


@coroutine
def setup_display_core_(var, config):
    if CONF_ROTATION in config:
        cg.add(var.set_rotation(DISPLAY_ROTATIONS[config[CONF_ROTATION]]))
    if CONF_PAGES in config:
        pages = []
        for conf in config[CONF_PAGES]:
            lambda_ = yield cg.process_lambda(
                conf[CONF_LAMBDA], [(DisplayBufferRef, "it")], return_type=cg.void
            )
            page = cg.new_Pvariable(conf[CONF_ID], lambda_)
            pages.append(page)
        cg.add(var.set_pages(pages))


@coroutine
def register_display(var, config):
    yield setup_display_core_(var, config)

    if CONF_BUFFER_ID in config:
        cg.add_define("USE_BUFFER")
        if CONF_BUFFER not in config:
            config[CONF_BUFFER_ID].type = buffer_565
            cg.add_define("USE_BUFFER_RGB565")
        else:
            cg.add_define("USE_BUFFER_" + config[CONF_BUFFER][CONF_TYPE])
            if config[CONF_BUFFER][CONF_TYPE] == "RGB666":
                config[CONF_BUFFER_ID].type = buffer_666
            if config[CONF_BUFFER][CONF_TYPE] == "RGB565":
                config[CONF_BUFFER_ID].type = buffer_565
            elif config[CONF_BUFFER][CONF_TYPE] == "RGB332":
                config[CONF_BUFFER_ID].type = buffer_332
            elif config[CONF_BUFFER][CONF_TYPE] == "INDEXED8":

                config[CONF_BUFFER_ID].type = buffer_indexed8

        buffer = yield cg.new_Pvariable(config[CONF_BUFFER_ID])
        cg.add(var.set_buffer_base(buffer))

        if CONF_WAVESHARE_COLORS in config:
            colors = []
            for color_conf in config[CONF_WAVESHARE_COLORS]:
                colors.append(WAVESHARE_COLORS_ENUM[color_conf[CONF_COLOR]])

            cg.add(buffer.set_colors(colors))

        index_size = 1

        if CONF_BUFFER in config and config[CONF_BUFFER][CONF_TYPE] == "INDEXED8":
            if CONF_INDEX_SIZE in config[CONF_BUFFER]:
                index_size = config[CONF_BUFFER][CONF_INDEX_SIZE]

            if CONF_COLORS in config[CONF_BUFFER]:
                colors = []
                for color_conf in config[CONF_BUFFER][CONF_COLORS]:
                    color_ = yield cg.get_variable(color_conf[CONF_COLOR])
                    colors.append(color_)
                cg.add(buffer.set_colors(colors))
                if CONF_INDEX_SIZE not in config[CONF_BUFFER]:
                    index_size = len(colors)

        if CONF_BUFFER_INDEX_SIZE in config:
            index_size = config[CONF_BUFFER_INDEX_SIZE]

        cg.add(buffer.set_index_size(index_size))

    if CONF_DEVICE_WIDTH in config:
        cg.add(var.set_device_width(config[CONF_DEVICE_WIDTH]))

    if CONF_DEVICE_HEIGHT in config:
        cg.add(var.set_device_height(config[CONF_DEVICE_HEIGHT]))

    if CONF_COL_START in config:
        cg.add(var.set_col_start(config[CONF_COL_START]))

    if CONF_ROW_START in config:
        cg.add(var.set_row_start(config[CONF_ROW_START]))


@automation.register_action(
    "display.page.show",
    DisplayPageShowAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.templatable(cv.use_id(DisplayPage)),
        }
    ),
)
def display_page_show_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    if isinstance(config[CONF_ID], core.Lambda):
        template_ = yield cg.templatable(config[CONF_ID], args, DisplayPagePtr)
        cg.add(var.set_page(template_))
    else:
        paren = yield cg.get_variable(config[CONF_ID])
        cg.add(var.set_page(paren))
    yield var


@automation.register_action(
    "display.page.show_next",
    DisplayPageShowNextAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.templatable(cv.use_id(DisplayBuffer)),
        }
    ),
)
def display_page_show_next_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


@automation.register_action(
    "display.page.show_previous",
    DisplayPageShowPrevAction,
    maybe_simple_id(
        {
            cv.Required(CONF_ID): cv.templatable(cv.use_id(DisplayBuffer)),
        }
    ),
)
def display_page_show_previous_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)


@coroutine_with_priority(100.0)
def to_code(config):
    cg.add_global(display_ns.using)
