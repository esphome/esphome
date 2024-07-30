import logging

from esphome.automation import build_automation, register_action, validate_automation
import esphome.codegen as cg
from esphome.components.display import Display
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_BUFFER_SIZE,
    CONF_ID,
    CONF_LAMBDA,
    CONF_ON_IDLE,
    CONF_PAGES,
    CONF_TIMEOUT,
    CONF_TRIGGER_ID,
    CONF_TYPE,
)
from esphome.core import CORE, ID, Lambda
from esphome.cpp_generator import MockObj
from esphome.final_validate import full_config
from esphome.helpers import write_file_if_changed

from . import defines as df, helpers, lv_validation as lvalid
from .automation import update_to_code
from .btn import btn_spec
from .label import label_spec
from .lv_validation import lv_images_used
from .lvcode import LvContext
from .obj import obj_spec
from .rotary_encoders import ROTARY_ENCODER_CONFIG, rotary_encoders_to_code
from .schemas import any_widget_schema, create_modify_schema, obj_schema
from .touchscreens import touchscreen_schema, touchscreens_to_code
from .trigger import generate_triggers
from .types import (
    WIDGET_TYPES,
    FontEngine,
    IdleTrigger,
    LvglComponent,
    ObjUpdateAction,
    lv_font_t,
    lvgl_ns,
)
from .widget import Widget, add_widgets, lv_scr_act, set_obj_properties

DOMAIN = "lvgl"
DEPENDENCIES = ("display",)
AUTO_LOAD = ("key_provider",)
CODEOWNERS = ("@clydebarrow",)
LOGGER = logging.getLogger(__name__)

for w_type in (label_spec, obj_spec, btn_spec):
    WIDGET_TYPES[w_type.name] = w_type

WIDGET_SCHEMA = any_widget_schema()

for w_type in WIDGET_TYPES.values():
    register_action(
        f"lvgl.{w_type.name}.update",
        ObjUpdateAction,
        create_modify_schema(w_type),
    )(update_to_code)


async def add_init_lambda(lv_component, init):
    if init:
        lamb = await cg.process_lambda(
            Lambda(init), [(LvglComponent.operator("ptr"), "lv_component")]
        )
        cg.add(lv_component.add_init_lambda(lamb))


lv_defines = {}  # Dict of #defines to provide as build flags


def add_define(macro, value="1"):
    if macro in lv_defines and lv_defines[macro] != value:
        LOGGER.error(
            "Redefinition of %s - was %s now %s", macro, lv_defines[macro], value
        )
    lv_defines[macro] = value


def as_macro(macro, value):
    if value is None:
        return f"#define {macro}"
    return f"#define {macro} {value}"


LV_CONF_FILENAME = "lv_conf.h"
LV_CONF_H_FORMAT = """\
#pragma once
{}
"""


def generate_lv_conf_h():
    definitions = [as_macro(m, v) for m, v in lv_defines.items()]
    definitions.sort()
    return LV_CONF_H_FORMAT.format("\n".join(definitions))


def final_validation(config):
    global_config = full_config.get()
    for display_id in config[df.CONF_DISPLAYS]:
        path = global_config.get_path_for_id(display_id)[:-1]
        display = global_config.get_config_for_path(path)
        if CONF_LAMBDA in display:
            raise cv.Invalid("Using lambda: in display config not compatible with LVGL")
        if display[CONF_AUTO_CLEAR_ENABLED]:
            raise cv.Invalid(
                "Using auto_clear_enabled: true in display config not compatible with LVGL"
            )
    buffer_frac = config[CONF_BUFFER_SIZE]
    if CORE.is_esp32 and buffer_frac > 0.5 and "psram" not in global_config:
        LOGGER.warning("buffer_size: may need to be reduced without PSRAM")
    for image_id in lv_images_used:
        path = global_config.get_path_for_id(image_id)[:-1]
        image_conf = global_config.get_config_for_path(path)
        if image_conf[CONF_TYPE] in ("RGBA", "RGB24"):
            raise cv.Invalid(
                "Using RGBA or RGB24 in image config not compatible with LVGL", path
            )


async def to_code(config):
    cg.add_library("lvgl/lvgl", "8.4.0")
    CORE.add_define("USE_LVGL")
    # suppress default enabling of extra widgets
    add_define("_LV_KCONFIG_PRESENT")
    # Always enable - lots of things use it.
    add_define("LV_DRAW_COMPLEX", "1")
    add_define("LV_TICK_CUSTOM", "1")
    add_define("LV_TICK_CUSTOM_INCLUDE", '"esphome/components/lvgl/lvgl_hal.h"')
    add_define("LV_TICK_CUSTOM_SYS_TIME_EXPR", "(lv_millis())")
    add_define("LV_MEM_CUSTOM", "1")
    add_define("LV_MEM_CUSTOM_ALLOC", "lv_custom_mem_alloc")
    add_define("LV_MEM_CUSTOM_FREE", "lv_custom_mem_free")
    add_define("LV_MEM_CUSTOM_REALLOC", "lv_custom_mem_realloc")
    add_define("LV_MEM_CUSTOM_INCLUDE", '"esphome/components/lvgl/lvgl_hal.h"')

    add_define("LV_LOG_LEVEL", f"LV_LOG_LEVEL_{config[df.CONF_LOG_LEVEL]}")
    add_define("LV_COLOR_DEPTH", config[df.CONF_COLOR_DEPTH])
    for font in helpers.lv_fonts_used:
        add_define(f"LV_FONT_{font.upper()}")

    if config[df.CONF_COLOR_DEPTH] == 16:
        add_define(
            "LV_COLOR_16_SWAP",
            "1" if config[df.CONF_BYTE_ORDER] == "big_endian" else "0",
        )
    add_define(
        "LV_COLOR_CHROMA_KEY",
        await lvalid.lv_color.process(config[df.CONF_TRANSPARENCY_KEY]),
    )
    CORE.add_build_flag("-Isrc")

    cg.add_global(lvgl_ns.using)
    lv_component = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(lv_component, config)
    Widget.create(config[CONF_ID], lv_component, obj_spec, config)
    for display in config[df.CONF_DISPLAYS]:
        cg.add(lv_component.add_display(await cg.get_variable(display)))

    frac = config[CONF_BUFFER_SIZE]
    if frac >= 0.75:
        frac = 1
    elif frac >= 0.375:
        frac = 2
    elif frac > 0.19:
        frac = 4
    else:
        frac = 8
    cg.add(lv_component.set_buffer_frac(int(frac)))
    cg.add(lv_component.set_full_refresh(config[df.CONF_FULL_REFRESH]))

    for font in helpers.esphome_fonts_used:
        await cg.get_variable(font)
        cg.new_Pvariable(ID(f"{font}_engine", True, type=FontEngine), MockObj(font))
    default_font = config[df.CONF_DEFAULT_FONT]
    if not lvalid.is_lv_font(default_font):
        add_define(
            "LV_FONT_CUSTOM_DECLARE", f"LV_FONT_DECLARE(*{df.DEFAULT_ESPHOME_FONT})"
        )
        globfont_id = ID(
            df.DEFAULT_ESPHOME_FONT,
            True,
            type=lv_font_t.operator("ptr").operator("const"),
        )
        cg.new_variable(
            globfont_id, MockObj(await lvalid.lv_font.process(default_font))
        )
        add_define("LV_FONT_DEFAULT", df.DEFAULT_ESPHOME_FONT)
    else:
        add_define("LV_FONT_DEFAULT", await lvalid.lv_font.process(default_font))

    with LvContext():
        await touchscreens_to_code(lv_component, config)
        await rotary_encoders_to_code(lv_component, config)
        await set_obj_properties(lv_scr_act, config)
        await add_widgets(lv_scr_act, config)
        Widget.set_completed()
        await generate_triggers(lv_component)
        for conf in config.get(CONF_ON_IDLE, ()):
            templ = await cg.templatable(conf[CONF_TIMEOUT], [], cg.uint32)
            idle_trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], lv_component, templ)
            await build_automation(idle_trigger, [], conf)
    await add_init_lambda(lv_component, LvContext.get_code())
    for comp in helpers.lvgl_components_required:
        CORE.add_define(f"USE_LVGL_{comp.upper()}")
    for use in helpers.lv_uses:
        add_define(f"LV_USE_{use.upper()}")
    lv_conf_h_file = CORE.relative_src_path(LV_CONF_FILENAME)
    write_file_if_changed(lv_conf_h_file, generate_lv_conf_h())
    CORE.add_build_flag("-DLV_CONF_H=1")
    CORE.add_build_flag(f'-DLV_CONF_PATH="{LV_CONF_FILENAME}"')


def display_schema(config):
    value = cv.ensure_list(cv.use_id(Display))(config)
    return value or [cv.use_id(Display)(config)]


FINAL_VALIDATE_SCHEMA = final_validation

CONFIG_SCHEMA = (
    cv.polling_component_schema("1s")
    .extend(obj_schema(obj_spec))
    .extend(
        {
            cv.GenerateID(CONF_ID): cv.declare_id(LvglComponent),
            cv.GenerateID(df.CONF_DISPLAYS): display_schema,
            cv.Optional(df.CONF_COLOR_DEPTH, default=16): cv.one_of(16),
            cv.Optional(df.CONF_DEFAULT_FONT, default="montserrat_14"): lvalid.lv_font,
            cv.Optional(df.CONF_FULL_REFRESH, default=False): cv.boolean,
            cv.Optional(CONF_BUFFER_SIZE, default="100%"): cv.percentage,
            cv.Optional(df.CONF_LOG_LEVEL, default="WARN"): cv.one_of(
                *df.LOG_LEVELS, upper=True
            ),
            cv.Optional(df.CONF_BYTE_ORDER, default="big_endian"): cv.one_of(
                "big_endian", "little_endian"
            ),
            cv.Optional(CONF_ON_IDLE): validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(IdleTrigger),
                    cv.Required(CONF_TIMEOUT): cv.templatable(
                        cv.positive_time_period_milliseconds
                    ),
                }
            ),
            cv.Optional(df.CONF_WIDGETS): cv.ensure_list(WIDGET_SCHEMA),
            cv.Optional(df.CONF_TRANSPARENCY_KEY, default=0x000400): lvalid.lv_color,
            cv.GenerateID(df.CONF_TOUCHSCREENS): touchscreen_schema,
            cv.GenerateID(df.CONF_ROTARY_ENCODERS): ROTARY_ENCODER_CONFIG,
        }
    )
).add_extra(cv.has_at_least_one_key(CONF_PAGES, df.CONF_WIDGETS))
