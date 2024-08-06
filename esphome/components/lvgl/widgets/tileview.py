from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_VALUE, CONF_ROW, CONF_TRIGGER_ID

from ..automation import action_to_code
from ..defines import (
    CONF_ANIMATED,
    CONF_COLUMN,
    CONF_DIR,
    CONF_MAIN,
    CONF_TILE_ID,
    CONF_TILES,
    TILE_DIRECTIONS,
    literal,
)
from ..lv_validation import animated, lv_int
from ..lvcode import lv, lv_assign, lv_expr, lv_obj, lv_Pvariable
from ..schemas import container_schema
from ..types import LV_EVENT, LvType, ObjUpdateAction, lv_obj_t, lv_obj_t_ptr
from . import Widget, WidgetType, add_widgets, get_widgets, set_obj_properties
from .obj import obj_spec

CONF_TILEVIEW = "tileview"

lv_tile_t = LvType("lv_tileview_tile_t")

lv_tileview_t = LvType(
    "lv_tileview_t",
    largs=[(lv_obj_t_ptr, "tile")],
    lvalue=lambda w: w.get_property("tile_act"),
)

tile_spec = WidgetType("lv_tileview_tile_t", lv_tile_t, (CONF_MAIN,), {})

TILEVIEW_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_TILES): cv.ensure_list(
            container_schema(
                obj_spec,
                {
                    cv.Required(CONF_ROW): lv_int,
                    cv.Required(CONF_COLUMN): lv_int,
                    cv.GenerateID(): cv.declare_id(lv_tile_t),
                    cv.Optional(CONF_DIR, default="ALL"): TILE_DIRECTIONS.several_of,
                },
            )
        ),
        cv.Optional(CONF_ON_VALUE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    automation.Trigger.template(lv_obj_t_ptr)
                )
            }
        ),
    }
)


class TileviewType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_TILEVIEW,
            lv_tileview_t,
            (CONF_MAIN,),
            schema=TILEVIEW_SCHEMA,
            modify_schema={},
        )

    async def to_code(self, w: Widget, config: dict):
        for tile_conf in config.get(CONF_TILES, ()):
            w_id = tile_conf[CONF_ID]
            tile_obj = lv_Pvariable(lv_obj_t, w_id)
            tile = Widget.create(w_id, tile_obj, tile_spec, tile_conf)
            dirs = tile_conf[CONF_DIR]
            if isinstance(dirs, list):
                dirs = "|".join(dirs)
            lv_assign(
                tile_obj,
                lv_expr.tileview_add_tile(
                    w.obj, tile_conf[CONF_COLUMN], tile_conf[CONF_ROW], literal(dirs)
                ),
            )
            await set_obj_properties(tile, tile_conf)
            await add_widgets(tile, tile_conf)


tileview_spec = TileviewType()


def tile_select_validate(config):
    row = CONF_ROW in config
    column = CONF_COLUMN in config
    tile = CONF_TILE_ID in config
    if tile and (row or column) or not tile and not (row and column):
        raise cv.Invalid("Specify either a tile id, or both a row and a column")
    return config


@automation.register_action(
    "lvgl.tileview.select",
    ObjUpdateAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(lv_tileview_t),
            cv.Optional(CONF_ANIMATED, default=False): animated,
            cv.Optional(CONF_ROW): lv_int,
            cv.Optional(CONF_COLUMN): lv_int,
            cv.Optional(CONF_TILE_ID): cv.use_id(lv_tile_t),
        },
    ).add_extra(tile_select_validate),
)
async def tileview_select(config, action_id, template_arg, args):
    widgets = await get_widgets(config)

    async def do_select(w: Widget):
        if tile := config.get(CONF_TILE_ID):
            tile = await cg.get_variable(tile)
            lv_obj.set_tile(w.obj, tile, literal(config[CONF_ANIMATED]))
        else:
            row = await lv_int.process(config[CONF_ROW])
            column = await lv_int.process(config[CONF_COLUMN])
            lv_obj.set_tile_id(
                widgets[0].obj, column, row, literal(config[CONF_ANIMATED])
            )
        lv.event_send(w.obj, LV_EVENT.VALUE_CHANGED, cg.nullptr)

    return await action_to_code(widgets, do_select, action_id, template_arg, args)
