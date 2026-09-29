"""
LVGL 9.4 Canvas Widget Implementation

This module implements the canvas widget for LVGL 9.4. Key changes from LVGL 8.4:

1. Buffer allocation:
   - LV_IMG_CF_TRUE_COLOR → LV_COLOR_FORMAT_RGB565
   - LV_IMG_CF_TRUE_COLOR_ALPHA → LV_COLOR_FORMAT_ARGB8888
   - LV_CANVAS_BUF_SIZE_TRUE_COLOR → LV_CANVAS_BUF_SIZE(w, h, bpp, stride)

2. Drawing API:
   - All lv_canvas_draw_* functions removed
   - Use layer-based drawing: lv_canvas_init_layer() / lv_canvas_finish_layer()
   - Draw using low-level lv_draw_* functions (rect, line, arc, image, label)

3. Pixel operations:
   - lv_canvas_set_px_color + lv_canvas_set_px_opa → lv_canvas_set_px(color, opa)
"""

from esphome import automation, codegen as cg, config_validation as cv
from esphome.components.display_menu_base import CONF_LABEL
from esphome.const import (
    CONF_COLOR,
    CONF_HEIGHT,
    CONF_ID,
    CONF_TEXT,
    CONF_WIDTH,
    CONF_X,
    CONF_Y,
)
from esphome.cpp_types import FixedVector

from ..automation import action_to_code
from ..defines import (
    CONF_END_ANGLE,
    CONF_MAIN,
    CONF_OPA,
    CONF_PIVOT_X,
    CONF_PIVOT_Y,
    CONF_POINTS,
    CONF_RADIUS,
    CONF_SRC,
    CONF_START_ANGLE,
    addr,
    literal,
)
from ..lv_validation import (
    lv_angle_degrees,
    lv_bool,
    lv_color,
    lv_image,
    lv_text,
    opacity,
    pixels,
    pixels_or_percent,
    size,
)
from ..lvcode import LocalVariable, lv, lv_assign, lv_expr
from ..schemas import STYLE_PROPS, TEXT_SCHEMA, point_schema, remap_property
from ..types import LvType, ObjUpdateAction, lv_point_precise_t
from . import Widget, WidgetType, get_widgets
from .img import CONF_IMAGE

CONF_CANVAS = "canvas"
CONF_BUFFER_ID = "buffer_id"
CONF_MAX_WIDTH = "max_width"
CONF_TRANSPARENT = "transparent"
CONF_DRAW_BUF_ID = "draw_buf_id"

lv_canvas_t = LvType("lv_canvas_t")
lv_draw_buf_t = LvType("lv_draw_buf_t")


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
                    cv.GenerateID(CONF_DRAW_BUF_ID): cv.declare_id(lv_draw_buf_t),
                }
            ),
            modify_schema={},
        )

    def get_uses(self):
        return CONF_IMAGE, CONF_LABEL

    async def to_code(self, w: Widget, config):
        width = config[CONF_WIDTH]
        height = config[CONF_HEIGHT]
        # LVGL 9.4: Use LV_COLOR_FORMAT instead of LV_IMG_CF
        # RGB565 is 16-bit (2 bytes per pixel), ARGB8888 is 32-bit (4 bytes per pixel)
        if config[CONF_TRANSPARENT]:
            color_format = "LV_COLOR_FORMAT_ARGB8888"
        else:
            color_format = "LV_COLOR_FORMAT_NATIVE"

        # LVGL 9.4: LV_CANVAS_BUF_SIZE(width, height, bits_per_pixel, stride)
        # stride is 0 for default (width * bytes_per_pixel)
        draw_buf = cg.new_Pvariable(config[CONF_DRAW_BUF_ID])
        buf_size = literal(f"LV_DRAW_BUF_SIZE({width}, {height}, {color_format})")
        lv.draw_buf_init(
            draw_buf,
            width,
            height,
            literal(color_format),
            0,
            lv_expr.malloc_core(buf_size),
            literal(buf_size),
        )
        lv.draw_buf_set_flag(draw_buf, literal("LV_IMAGE_FLAGS_MODIFIABLE"))
        lv.canvas_set_draw_buf(w.obj, draw_buf)


CanvasType()


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
    synchronous=True,
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
            cv.Optional(CONF_OPA, default="COVER"): opacity,
            cv.Required(CONF_POINTS): cv.ensure_list(point_schema),
        },
    ),
    synchronous=True,
)
async def canvas_set_pixel(config, action_id, template_arg, args):
    widget = await get_widgets(config)
    color = await lv_color.process(config[CONF_COLOR])
    opa = await opacity.process(config.get(CONF_OPA), "COVER")
    points = [
        (
            await pixels.process(p[CONF_X]),
            await pixels.process(p[CONF_Y]),
        )
        for p in config[CONF_POINTS]
    ]

    async def do_set_pixels(w: Widget):
        # LVGL 9.4: lv_canvas_set_px combines color and opacity
        # Could optimize this for lambda values
        for point in points:
            x, y = point
            lv.canvas_set_px(w.obj, x, y, color, opa)

    return await action_to_code(
        widget, do_set_pixels, action_id, template_arg, args, config
    )


DRAW_SCHEMA = {
    cv.GenerateID(CONF_ID): cv.use_id(lv_canvas_t),
    cv.Required(CONF_X): pixels,
    cv.Required(CONF_Y): pixels,
}
DRAW_OPA_SCHEMA = {
    **DRAW_SCHEMA,
    cv.Optional(CONF_OPA): opacity,
}


async def draw_to_code(config, dsc_type, props, do_draw, action_id, template_arg, args):
    widget = await get_widgets(config)
    x = await pixels.process(config.get(CONF_X))
    y = await pixels.process(config.get(CONF_Y))

    async def action_func(w: Widget):
        # LVGL 9.4: Create a layer for drawing on canvas
        with LocalVariable("layer", "lv_layer_t", modifier="") as layer:
            lv.canvas_init_layer(w.obj, addr(layer))
            with LocalVariable("dsc", f"lv_draw_{dsc_type}_dsc_t", modifier="") as dsc:
                lv.call(f"draw_{dsc_type}_dsc_init", addr(dsc))
                if CONF_OPA in config:
                    opa = await opacity.process(config[CONF_OPA])
                    lv_assign(dsc.opa, opa)
                for prop, validator in props.items():
                    if prop in config:
                        value = await validator.process(config[prop])
                        mapped_prop = remap_property(prop)
                        lv_assign(getattr(dsc, mapped_prop), value)
                await do_draw(addr(layer), x, y, dsc)
            lv.canvas_finish_layer(w.obj, addr(layer))

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
        "shadow_offset_x",
        "shadow_offset_y",
        "shadow_ofs_x",
        "shadow_ofs_y",
        "shadow_spread",
        "shadow_opa",
    )
}


def _draw_line(layer, dsc, points):
    # LVGL 9.4: Use lv_draw_line for each line segment
    with (
        LocalVariable(
            "points", FixedVector.template(lv_point_precise_t), points, modifier=""
        ) as points_var,
        LocalVariable("i", "uint32_t", literal("0"), modifier="") as i,
    ):
        # Draw lines between consecutive points
        lv.append(
            cg.RawStatement(f"for ({i} = 0; {i} != {points_var}.size() - 1; {i}++) {{")
        )
        lv_assign(dsc.p1, points_var[i])
        lv_assign(dsc.p2, points_var[i + 1])
        lv.draw_line(layer, addr(dsc))
        lv.append(cg.RawStatement("}"))


@automation.register_action(
    "lvgl.canvas.draw_rectangle",
    ObjUpdateAction,
    cv.Schema(
        {
            **DRAW_OPA_SCHEMA,
            cv.Required(CONF_WIDTH): cv.templatable(cv.int_),
            cv.Required(CONF_HEIGHT): cv.templatable(cv.int_),
            **{cv.Optional(prop): STYLE_PROPS[prop] for prop in RECT_PROPS},
        }
    ),
    synchronous=True,
)
async def canvas_draw_rect(config, action_id, template_arg, args):
    width = await pixels.process(config[CONF_WIDTH])
    height = await pixels.process(config[CONF_HEIGHT])

    async def do_draw_rect(layer, x, y, dsc):
        # LVGL 9.4: Use lv_draw_rect with area
        with LocalVariable("area", "lv_area_t", modifier="") as area:
            lv_assign(area.x1, x)
            lv_assign(area.y1, y)
            lv_assign(area.x2, literal(f"{x} + {width} - 1"))
            lv_assign(area.y2, literal(f"{y} + {height} - 1"))
            lv.draw_rect(layer, addr(dsc), addr(area))

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
    cv.Schema(
        {
            **TEXT_SCHEMA,
            **DRAW_OPA_SCHEMA,
            cv.Required(CONF_MAX_WIDTH): cv.templatable(cv.int_),
            **{cv.Optional(prop): STYLE_PROPS[f"text_{prop}"] for prop in TEXT_PROPS},
        },
    ),
    synchronous=True,
)
async def canvas_draw_text(config, action_id, template_arg, args):
    text = await lv_text.process(config[CONF_TEXT])
    max_width = await pixels.process(config[CONF_MAX_WIDTH])

    async def do_draw_text(layer, x, y, dsc):
        # LVGL 9.4: Use lv_draw_label with area and hint
        with LocalVariable("area", "lv_area_t", modifier="") as area:
            lv_assign(area.x1, x)
            lv_assign(area.y1, y)
            lv_assign(area.x2, literal(f"{x} + {max_width} - 1"))
            lv_assign(area.y2, literal(f"{y} + LV_COORD_MAX"))
            lv_assign(dsc.text, text)
            lv.draw_label(layer, addr(dsc), addr(area))

    return await draw_to_code(
        config, "label", TEXT_PROPS, do_draw_text, action_id, template_arg, args
    )


IMG_PROPS = (
    "angle",
    "rotation",
    "scale_x",
    "scale_y",
    "skew_x",
    "skew_y",
    "scale",
    "zoom",
    "recolor",
    "recolor_opa",
    "opa",
)


def _scale_map(config):
    config = {remap_property(p): v for p, v in config.items()}
    if "scale" in config and {"scale_x", "scale_y"} & config.keys():
        raise cv.Invalid("Cannot specify both scale and scale_x/scale_y")
    if "scale" in config:
        config.update({"scale_x": config["scale"], "scale_y": config["scale"]})
        del config["scale"]
    return config


def _get_prop_validator(prop):
    return STYLE_PROPS.get(
        f"transform_{remap_property(prop, False)}"
    ) or STYLE_PROPS.get(prop)


def _prop_validator(prop):
    def validator(value):
        return _get_prop_validator(prop)(value)

    return validator


@automation.register_action(
    "lvgl.canvas.draw_image",
    ObjUpdateAction,
    cv.Schema(
        {
            **DRAW_OPA_SCHEMA,
            cv.Required(CONF_SRC): lv_image,
            cv.Optional(CONF_PIVOT_X, default=0): pixels,
            cv.Optional(CONF_PIVOT_Y, default=0): pixels,
            **{cv.Optional(prop): _prop_validator(prop) for prop in IMG_PROPS},
        }
    ).add_extra(_scale_map),
    synchronous=True,
)
async def canvas_draw_image(config, action_id, template_arg, args):
    src = await lv_image.process(config[CONF_SRC])
    pivot_x = await pixels.process(config[CONF_PIVOT_X])
    pivot_y = await pixels.process(config[CONF_PIVOT_Y])

    async def do_draw_image(layer, x, y, dsc):
        # LVGL 9.4: Use lv_draw_image with area
        lv_assign(dsc.src, src.get_lv_image_dsc())
        if pivot_x or pivot_y:
            # pylint :disable=no-member
            lv_assign(dsc.pivot.x, pivot_x)
            lv_assign(dsc.pivot.y, pivot_y)
        with LocalVariable("area", "lv_area_t", modifier="") as area:
            lv_assign(area.x1, x)
            lv_assign(area.y1, y)
            # Image size will be determined from the image descriptor
            lv_assign(area.x2, x)
            lv_assign(area.y2, y)
            lv.draw_image(layer, addr(dsc), addr(area))

    return await draw_to_code(
        config,
        "image",
        {prop: _get_prop_validator(prop) for prop in IMG_PROPS},
        do_draw_image,
        action_id,
        template_arg,
        args,
    )


LINE_PROPS = {
    "width": STYLE_PROPS["line_width"],
    "color": STYLE_PROPS["line_color"],
    "dash-width": STYLE_PROPS["line_dash_width"],
    "dash-gap": STYLE_PROPS["line_dash_gap"],
    "round_start": lv_bool,
    "round_end": lv_bool,
}


def _validate_points(config):
    for index, point in enumerate(config[CONF_POINTS]):
        if not all(isinstance(p, int) for p in point.values()):
            raise cv.Invalid("Points must be integers", path=[CONF_POINTS, index])
    return config


@automation.register_action(
    "lvgl.canvas.draw_line",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.GenerateID(CONF_ID): cv.use_id(lv_canvas_t),
            cv.Optional(CONF_OPA): opacity,
            cv.Required(CONF_POINTS): cv.ensure_list(point_schema),
            **{cv.Optional(prop): validator for prop, validator in LINE_PROPS.items()},
        }
    ).add_extra(_validate_points),
    synchronous=True,
)
async def canvas_draw_line(config, action_id, template_arg, args):
    points = [
        [
            await pixels.process(p[CONF_X]),
            await pixels.process(p[CONF_Y]),
        ]
        for p in config[CONF_POINTS]
    ]

    async def do_draw_line(layer, _x, _y, dsc):
        _draw_line(layer, dsc, points)

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
            **{cv.Optional(prop): STYLE_PROPS[prop] for prop in RECT_PROPS},
        },
    ).add_extra(_validate_points),
    synchronous=True,
)
async def canvas_draw_polygon(config, action_id, template_arg, args):
    points = [
        [
            await pixels_or_percent.process(p[CONF_X]),
            await pixels_or_percent.process(p[CONF_Y]),
        ]
        for p in config[CONF_POINTS]
    ]
    # Close the polygon
    points.append(points[0])

    async def do_draw_polygon(layer, x, y, dsc):
        # LVGL 9.4: Draw polygon using line drawing in a closed path
        # Note: This draws outline only. For filled polygons, would need different approach
        # Convert rect descriptor to line descriptor for polygon outline
        with LocalVariable("line_dsc", "lv_draw_line_dsc_t", modifier="") as line_dsc:
            lv.draw_line_dsc_init(addr(line_dsc))
            # Copy border properties from rect descriptor to line descriptor
            lv_assign(line_dsc.color, dsc.border_color)
            lv_assign(line_dsc.width, dsc.border_width)
            lv_assign(line_dsc.opa, dsc.border_opa)
            _draw_line(layer, line_dsc, points)

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
    cv.Schema(
        {
            **DRAW_OPA_SCHEMA,
            cv.Required(CONF_RADIUS): pixels,
            cv.Required(CONF_START_ANGLE): lv_angle_degrees,
            cv.Required(CONF_END_ANGLE): lv_angle_degrees,
            **{cv.Optional(prop): validator for prop, validator in ARC_PROPS.items()},
        }
    ),
    synchronous=True,
)
async def canvas_draw_arc(config, action_id, template_arg, args):
    radius = await size.process(config[CONF_RADIUS])
    start_angle = await lv_angle_degrees.process(config[CONF_START_ANGLE])
    end_angle = await lv_angle_degrees.process(config[CONF_END_ANGLE])

    async def do_draw_arc(layer, x, y, dsc):
        # LVGL 9.4: Use lv_draw_arc with center point
        lv_assign(dsc.center.x, x)
        lv_assign(dsc.center.y, y)
        lv_assign(dsc.start_angle, start_angle)
        lv_assign(dsc.end_angle, end_angle)
        lv_assign(dsc.radius, radius)
        lv.draw_arc(layer, addr(dsc))

    return await draw_to_code(
        config, "arc", ARC_PROPS, do_draw_arc, action_id, template_arg, args
    )
