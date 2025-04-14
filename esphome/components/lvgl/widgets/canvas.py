from esphome import automation, codegen as cg, config_validation as cv
from esphome.components.display_menu_base import CONF_LABEL
from esphome.const import CONF_COLOR, CONF_HEIGHT, CONF_ID, CONF_TEXT, CONF_WIDTH
from esphome.cpp_generator import Literal, MockObj

from ..automation import action_to_code
from ..defines import (
    CONF_END_ANGLE,
    CONF_MAIN,
    CONF_OPA,
    CONF_PIVOT_X,
    CONF_PIVOT_Y,
    CONF_POINTS,
    CONF_SRC,
    CONF_START_ANGLE,
    CONF_X,
    CONF_Y,
    literal,
)
from ..lv_validation import (
    lv_angle,
    lv_bool,
    lv_color,
    lv_image,
    lv_text,
    opacity,
    pixels,
    size,
)
from ..lvcode import LocalVariable, lv, lv_assign
from ..schemas import STYLE_PROPS, STYLE_REMAP, TEXT_SCHEMA, point_schema
from ..types import LvType, ObjUpdateAction, WidgetType
from . import Widget, get_widgets
from .line import lv_point_t, process_coord

CONF_CANVAS = "canvas"
CONF_BUFFER_ID = "buffer_id"
CONF_MAX_WIDTH = "max_width"
CONF_TRANSPARENT = "transparent"
CONF_RADIUS = "radius"

lv_canvas_t = LvType("lv_canvas_t")


class CanvasType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_CANVAS,
            lv_canvas_t,
            (CONF_MAIN,),
            cv.Schema(
                {
                    cv.Required(CONF_WIDTH): size,
                    cv.Required(CONF_HEIGHT): size,
                    cv.Optional(CONF_TRANSPARENT, default=False): cv.boolean,
                }
            ),
        )

    def get_uses(self):
        return "img", CONF_LABEL

    async def to_code(self, w: Widget, config):
        width = config[CONF_WIDTH]
        height = config[CONF_HEIGHT]
        use_alpha = "_ALPHA" if config[CONF_TRANSPARENT] else ""
        lv.canvas_set_buffer(
            w.obj,
            lv.custom_mem_alloc(
                literal(f"LV_CANVAS_BUF_SIZE_TRUE_COLOR{use_alpha}({width}, {height})")
            ),
            width,
            height,
            literal(f"LV_IMG_CF_TRUE_COLOR{use_alpha}"),
        )


canvas_spec = CanvasType()


@automation.register_action(
    "lvgl.canvas.fill",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(lv_canvas_t),
            cv.Required(CONF_COLOR): lv_color,
            cv.Optional(CONF_OPA, default="COVER"): opacity,
        },
    ),
)
async def canvas_fill(config, action_id, template_arg, args):
    widget = await get_widgets(config)
    color = await lv_color.process(config[CONF_COLOR])
    opa = await opacity.process(config[CONF_OPA])

    async def do_fill(w: Widget):
        lv.canvas_fill_bg(w.obj, color, opa)

    return await action_to_code(widget, do_fill, action_id, template_arg, args, config)


@automation.register_action(
    "lvgl.canvas.set_pixels",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(lv_canvas_t),
            cv.Required(CONF_COLOR): lv_color,
            cv.Optional(CONF_OPA): opacity,
            cv.Required(CONF_POINTS): cv.ensure_list(point_schema),
        },
    ),
)
async def canvas_set_pixel(config, action_id, template_arg, args):
    widget = await get_widgets(config)
    color = await lv_color.process(config[CONF_COLOR])
    opa = await opacity.process(config.get(CONF_OPA))
    points = [
        (
            await pixels.process(p[CONF_X]),
            await pixels.process(p[CONF_Y]),
        )
        for p in config[CONF_POINTS]
    ]

    async def do_set_pixels(w: Widget):
        if isinstance(color, MockObj):
            for point in points:
                x, y = point
                lv.canvas_set_px_color(w.obj, x, y, color)
        else:
            with LocalVariable("color", "lv_color_t", color, modifier="") as color_var:
                for point in points:
                    x, y = point
                    lv.canvas_set_px_color(w.obj, x, y, color_var)
        if opa:
            if isinstance(opa, Literal):
                for point in points:
                    x, y = point
                    lv.canvas_set_px_opa(w.obj, x, y, opa)
            else:
                with LocalVariable("opa", "lv_opa_t", opa, modifier="") as opa_var:
                    for point in points:
                        x, y = point
                        lv.canvas_set_px_opa(w.obj, x, y, opa_var)

    return await action_to_code(
        widget, do_set_pixels, action_id, template_arg, args, config
    )


DRAW_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.use_id(lv_canvas_t),
        cv.Required(CONF_X): pixels,
        cv.Required(CONF_Y): pixels,
    }
)
DRAW_OPA_SCHEMA = DRAW_SCHEMA.extend(
    {
        cv.Optional(CONF_OPA): opacity,
    }
)


async def draw_to_code(config, dsc_type, props, do_draw, action_id, template_arg, args):
    widget = await get_widgets(config)
    x = await pixels.process(config.get(CONF_X))
    y = await pixels.process(config.get(CONF_Y))

    async def action_func(w: Widget):
        with LocalVariable("dsc", f"lv_draw_{dsc_type}_dsc_t", modifier="") as dsc:
            dsc_addr = literal(f"&{dsc}")
            lv.call(f"draw_{dsc_type}_dsc_init", dsc_addr)
            if CONF_OPA in config:
                opa = await opacity.process(config[CONF_OPA])
                lv_assign(dsc.opa, opa)
            for prop, validator in props.items():
                if prop in config:
                    value = await validator.process(config[prop])
                    mapped_prop = STYLE_REMAP.get(prop, prop)
                    lv_assign(getattr(dsc, mapped_prop), value)
            await do_draw(w, x, y, dsc_addr)

    return await action_to_code(
        widget, action_func, action_id, template_arg, args, config
    )


RECT_PROPS = {
    p: STYLE_PROPS[p]
    for p in (
        "radius",
        "bg_opa",
        "bg_color",
        "bg_grad",
        "border_color",
        "border_width",
        "border_opa",
        "outline_color",
        "outline_width",
        "outline_pad",
        "outline_opa",
        "shadow_color",
        "shadow_width",
        "shadow_ofs_x",
        "shadow_ofs_y",
        "shadow_spread",
        "shadow_opa",
    )
}


@automation.register_action(
    "lvgl.canvas.draw_rectangle",
    ObjUpdateAction,
    DRAW_SCHEMA.extend(
        {
            cv.Required(CONF_WIDTH): cv.templatable(cv.int_),
            cv.Required(CONF_HEIGHT): cv.templatable(cv.int_),
        },
    ).extend({cv.Optional(prop): STYLE_PROPS[prop] for prop in RECT_PROPS}),
)
async def canvas_draw_rect(config, action_id, template_arg, args):
    width = await pixels.process(config[CONF_WIDTH])
    height = await pixels.process(config[CONF_HEIGHT])

    async def do_draw_rect(w: Widget, x, y, dsc_addr):
        lv.canvas_draw_rect(w.obj, x, y, width, height, dsc_addr)

    return await draw_to_code(
        config, "rect", RECT_PROPS, do_draw_rect, action_id, template_arg, args
    )


TEXT_PROPS = {
    p: STYLE_PROPS[f"text_{p}"]
    for p in (
        "font",
        "color",
        # "sel_color",
        # "sel_bg_color",
        "line_space",
        "letter_space",
        "align",
        "decor",
    )
}


@automation.register_action(
    "lvgl.canvas.draw_text",
    ObjUpdateAction,
    TEXT_SCHEMA.extend(DRAW_OPA_SCHEMA)
    .extend(
        {
            cv.Required(CONF_MAX_WIDTH): cv.templatable(cv.int_),
        },
    )
    .extend({cv.Optional(prop): STYLE_PROPS[f"text_{prop}"] for prop in TEXT_PROPS}),
)
async def canvas_draw_text(config, action_id, template_arg, args):
    text = await lv_text.process(config[CONF_TEXT])
    max_width = await pixels.process(config[CONF_MAX_WIDTH])

    async def do_draw_text(w: Widget, x, y, dsc_addr):
        lv.canvas_draw_text(w.obj, x, y, max_width, dsc_addr, text)

    return await draw_to_code(
        config, "label", TEXT_PROPS, do_draw_text, action_id, template_arg, args
    )


IMG_PROPS = {
    "angle": STYLE_PROPS["transform_angle"],
    "zoom": STYLE_PROPS["transform_zoom"],
    "recolor": STYLE_PROPS["image_recolor"],
    "recolor_opa": STYLE_PROPS["image_recolor_opa"],
    "opa": STYLE_PROPS["opa"],
}


@automation.register_action(
    "lvgl.canvas.draw_image",
    ObjUpdateAction,
    DRAW_OPA_SCHEMA.extend(
        {
            cv.Required(CONF_SRC): lv_image,
            cv.Optional(CONF_PIVOT_X, default=0): pixels,
            cv.Optional(CONF_PIVOT_Y, default=0): pixels,
        },
    ).extend({cv.Optional(prop): validator for prop, validator in IMG_PROPS.items()}),
)
async def canvas_draw_image(config, action_id, template_arg, args):
    src = await lv_image.process(config[CONF_SRC])
    pivot_x = await pixels.process(config[CONF_PIVOT_X])
    pivot_y = await pixels.process(config[CONF_PIVOT_Y])

    async def do_draw_image(w: Widget, x, y, dsc_addr):
        dsc = MockObj(f"(*{dsc_addr})")
        if pivot_x or pivot_y:
            # pylint :disable=no-member
            lv_assign(dsc.pivot, literal(f"{{{pivot_x}, {pivot_y}}}"))
        lv.canvas_draw_img(w.obj, x, y, src, dsc_addr)

    return await draw_to_code(
        config, "img", IMG_PROPS, do_draw_image, action_id, template_arg, args
    )


LINE_PROPS = {
    "width": STYLE_PROPS["line_width"],
    "color": STYLE_PROPS["line_color"],
    "dash-width": STYLE_PROPS["line_dash_width"],
    "dash-gap": STYLE_PROPS["line_dash_gap"],
    "round_start": lv_bool,
    "round_end": lv_bool,
}


@automation.register_action(
    "lvgl.canvas.draw_line",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(lv_canvas_t),
            cv.Optional(CONF_OPA): opacity,
            cv.Required(CONF_POINTS): cv.ensure_list(point_schema),
        },
    ).extend({cv.Optional(prop): validator for prop, validator in LINE_PROPS.items()}),
)
async def canvas_draw_line(config, action_id, template_arg, args):
    points = [
        [await process_coord(p[CONF_X]), await process_coord(p[CONF_Y])]
        for p in config[CONF_POINTS]
    ]

    async def do_draw_line(w: Widget, x, y, dsc_addr):
        with LocalVariable(
            "points", cg.std_vector.template(lv_point_t), points, modifier=""
        ) as points_var:
            lv.canvas_draw_line(w.obj, points_var.data(), points_var.size(), dsc_addr)

    return await draw_to_code(
        config, "line", LINE_PROPS, do_draw_line, action_id, template_arg, args
    )


@automation.register_action(
    "lvgl.canvas.draw_polygon",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(lv_canvas_t),
            cv.Required(CONF_POINTS): cv.ensure_list(point_schema),
        },
    ).extend({cv.Optional(prop): STYLE_PROPS[prop] for prop in RECT_PROPS}),
)
async def canvas_draw_polygon(config, action_id, template_arg, args):
    points = [
        [await process_coord(p[CONF_X]), await process_coord(p[CONF_Y])]
        for p in config[CONF_POINTS]
    ]

    async def do_draw_polygon(w: Widget, x, y, dsc_addr):
        with LocalVariable(
            "points", cg.std_vector.template(lv_point_t), points, modifier=""
        ) as points_var:
            lv.canvas_draw_polygon(
                w.obj, points_var.data(), points_var.size(), dsc_addr
            )

    return await draw_to_code(
        config, "rect", RECT_PROPS, do_draw_polygon, action_id, template_arg, args
    )


ARC_PROPS = {
    "width": STYLE_PROPS["arc_width"],
    "color": STYLE_PROPS["arc_color"],
    "rounded": STYLE_PROPS["arc_rounded"],
}


@automation.register_action(
    "lvgl.canvas.draw_arc",
    ObjUpdateAction,
    DRAW_OPA_SCHEMA.extend(
        {
            cv.Required(CONF_RADIUS): pixels,
            cv.Required(CONF_START_ANGLE): lv_angle,
            cv.Required(CONF_END_ANGLE): lv_angle,
        }
    ).extend({cv.Optional(prop): validator for prop, validator in ARC_PROPS.items()}),
)
async def canvas_draw_arc(config, action_id, template_arg, args):
    radius = await size.process(config[CONF_RADIUS])
    start_angle = await lv_angle.process(config[CONF_START_ANGLE])
    end_angle = await lv_angle.process(config[CONF_END_ANGLE])

    async def do_draw_arc(w: Widget, x, y, dsc_addr):
        lv.canvas_draw_arc(w.obj, x, y, radius, start_angle, end_angle, dsc_addr)

    return await draw_to_code(
        config, "arc", ARC_PROPS, do_draw_arc, action_id, template_arg, args
    )
