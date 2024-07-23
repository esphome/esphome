from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ON_VALUE, CONF_ROW, CONF_TRIGGER_ID

from . import lv_validation as lv
from .codegen import action_to_code, add_widgets, set_obj_properties
from .defines import (
    CONF_ANIMATED,
    CONF_COLUMN,
    CONF_DIR,
    CONF_OBJ,
    CONF_TILE_ID,
    CONF_TILES,
    CONF_TILEVIEW,
    TILE_DIRECTIONS,
)
from .schemas import container_schema
from .types import ObjUpdateAction, lv_obj_t, lv_obj_t_ptr, lv_tile_t, lv_tileview_t
from .widget import Widget, WidgetType, get_widget


class TileviewType(WidgetType):
    def __init__(self):
        super().__init__(
            CONF_TILEVIEW,
            {
                cv.Required(CONF_TILES): cv.ensure_list(
                    container_schema(
                        CONF_OBJ,
                        {
                            cv.Required(CONF_ROW): lv.lv_int,
                            cv.Required(CONF_COLUMN): lv.lv_int,
                            cv.GenerateID(): cv.declare_id(lv_tile_t),
                            cv.Optional(
                                CONF_DIR, default="ALL"
                            ): TILE_DIRECTIONS.several_of,
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
            },
        )

    @property
    def w_type(self):
        return lv_tileview_t

    async def to_code(self, w: Widget, config: dict):
        init = []
        for tile_conf in config[CONF_TILES]:
            w_id = tile_conf[CONF_ID]
            tile_obj = cg.Pvariable(w_id, cg.nullptr, type_=lv_obj_t)
            tile = Widget.create(w_id, tile_obj, lv_tile_t)
            dirs = tile_conf[CONF_DIR]
            if isinstance(dirs, list):
                dirs = "|".join(dirs)
            init.append(
                f"{tile.obj} = lv_tileview_add_tile({w.obj}, {tile_conf[CONF_COLUMN]}, {tile_conf[CONF_ROW]}, {dirs})"
            )
            init.extend(await set_obj_properties(tile, tile_conf))
            init.extend(await add_widgets(tile, tile_conf))
        return init


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
            cv.Optional(CONF_ANIMATED, default=False): lv.animated,
            cv.Optional(CONF_ROW): lv.lv_int,
            cv.Optional(CONF_COLUMN): lv.lv_int,
            cv.Optional(CONF_TILE_ID): cv.use_id(lv_tile_t),
        },
    ).add_extra(tile_select_validate),
)
async def tileview_select(config, action_id, template_arg, args):
    widget = await get_widget(config[CONF_ID])
    if tile := config.get(CONF_TILE_ID):
        tile = await cg.get_variable(tile)
        init = [f"lv_obj_set_tile({widget.obj}, {tile}, {config[CONF_ANIMATED]})"]
    else:
        row = await lv.lv_int.process(config[CONF_ROW])
        column = await lv.lv_int.process(config[CONF_COLUMN])
        init = [
            f"lv_obj_set_tile_id({widget.obj}, {column}, {row}, {config[CONF_ANIMATED]})",
            f" lv_event_send({widget.obj}, LV_EVENT_VALUE_CHANGED, nullptr);",
        ]
    return await action_to_code(init, action_id, widget, template_arg, args)
