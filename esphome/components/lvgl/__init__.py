import logging

from esphome import automation
import esphome.codegen as cg
from esphome.components.binary_sensor import BinarySensor
from esphome.components.display import Display
from esphome.components.image import Image_
from esphome.components.rotary_encoder.sensor import RotaryEncoderSensor
from esphome.components.touchscreen import CONF_TOUCHSCREEN_ID, Touchscreen
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_BUFFER_SIZE,
    CONF_DISPLAY,
    CONF_DISPLAY_ID,
    CONF_GROUP,
    CONF_ID,
    CONF_LAMBDA,
    CONF_NAME,
    CONF_ON_IDLE,
    CONF_ON_VALUE,
    CONF_PAGES,
    CONF_SENSOR,
    CONF_TIMEOUT,
    CONF_TRIGGER_ID,
)
from esphome.core import CORE, ID, Lambda
from esphome.cpp_generator import LambdaExpression
from esphome.helpers import write_file_if_changed

from . import defines as df, helpers, lv_validation as lv, types as ty
from .animimg import animimg_spec
from .arc import arc_spec
from .btn import btn_spec
from .btnmatrix import BTNM_BTN_SCHEMA, btnmatrix_spec, get_button_data, set_btn_data
from .checkbox import checkbox_spec
from .codegen import (
    action_to_code,
    add_group,
    cgen,
    set_obj_properties,
    update_to_code,
    widget_to_code,
)
from .dropdown import dropdown_spec
from .helpers import get_line_marks, join_lines
from .img import img_spec
from .keyboard import keyboard_spec
from .label import label_spec
from .led import led_spec
from .line import line_spec
from .lv_bar import bar_spec
from .lv_switch import switch_spec

# from .menu import menu_spec
from .meter import meter_spec
from .obj import obj_spec
from .page import page_spec
from .roller import roller_spec
from .schemas import (
    ACTION_SCHEMA,
    ALL_STYLES,
    ENCODER_SCHEMA,
    FLEX_OBJ_SCHEMA,
    GRID_CELL_SCHEMA,
    LAYOUT_SCHEMAS,
    LVGL_SCHEMA,
    STYLE_SCHEMA,
    STYLED_TEXT_SCHEMA,
    WIDGET_TYPES,
    any_widget_schema,
    container_schema,
    create_modify_schema,
    grid_alignments,
    obj_schema,
)
from .slider import slider_spec
from .spinbox import spinbox_spec
from .spinner import spinner_spec
from .tabview import tabview_spec
from .textarea import textarea_spec
from .tileview import tileview_spec
from .types import ObjUpdateAction
from .widget import (
    LvScrActType,
    Widget,
    add_temp_var,
    get_widget,
    lv_temp_vars,
    theme_widget_map,
    widget_map,
)

DOMAIN = "lvgl"
DEPENDENCIES = ("display",)
AUTO_LOAD = ("key_provider",)
CODEOWNERS = ("@clydebarrow",)
LOGGER = logging.getLogger(__name__)

for widg in (
    animimg_spec,
    arc_spec,
    btn_spec,
    bar_spec,
    btnmatrix_spec,
    # chart_spec,
    checkbox_spec,
    dropdown_spec,
    img_spec,
    keyboard_spec,
    label_spec,
    led_spec,
    line_spec,
    # menu_spec,
    meter_spec,
    obj_spec,
    page_spec,
    roller_spec,
    slider_spec,
    spinner_spec,
    switch_spec,
    spinbox_spec,
    tabview_spec,
    textarea_spec,
    tileview_spec,
):
    WIDGET_TYPES[widg.name] = widg

lv_scr_act_spec = LvScrActType()
lv_scr_act = Widget.create(
    None, "lv_scr_act()", lv_scr_act_spec, {}, obj="lv_scr_act()", parent=None
)

WIDGET_SCHEMA = any_widget_schema()

LAYOUT_SCHEMAS[df.TYPE_GRID] = {
    cv.Optional(df.CONF_WIDGETS): cv.ensure_list(any_widget_schema(GRID_CELL_SCHEMA))
}
LAYOUT_SCHEMAS[df.TYPE_FLEX] = {
    cv.Optional(df.CONF_WIDGETS): cv.ensure_list(any_widget_schema(FLEX_OBJ_SCHEMA))
}
LAYOUT_SCHEMAS[df.TYPE_NONE] = {
    cv.Optional(df.CONF_WIDGETS): cv.ensure_list(any_widget_schema())
}


async def widget_update_to_code(config, action_id, template_arg, args):
    w = await get_widget(config[CONF_ID])
    init = await w.type.to_code(w, config)
    return await update_to_code(config, action_id, w, init, template_arg, args)


for w_name, w_type in WIDGET_TYPES.items():
    automation.register_action(
        f"lvgl.{w_name}.update",
        ObjUpdateAction,
        create_modify_schema(w_name, w_type.w_type, extras=w_type.modify_schema),
    )(widget_update_to_code)

MSGBOX_SCHEMA = STYLE_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(ty.lv_obj_t),
        cv.Required(df.CONF_TITLE): STYLED_TEXT_SCHEMA,
        cv.Optional(df.CONF_BODY): STYLED_TEXT_SCHEMA,
        cv.Optional(df.CONF_BUTTONS): cv.ensure_list(BTNM_BTN_SCHEMA),
        cv.Optional(df.CONF_CLOSE_BUTTON): lv.lv_bool,
        cv.Optional(df.CONF_WIDGETS): cv.ensure_list(WIDGET_SCHEMA),
    }
)


async def get_color_value(value):
    if isinstance(value, Lambda):
        return f"{await cg.process_lambda(value, [], return_type=ty.lv_color_t)}()"
    return value


async def add_init_lambda(lv_component, init):
    if init:
        lamb = await cg.process_lambda(
            Lambda(join_lines(init)), [(ty.lv_disp_t_ptr, "lv_disp")]
        )
        cg.add(lv_component.add_init_lambda(lamb))
    lv_temp_vars.clear()


async def styles_to_code(styles):
    """Convert styles to C__ code."""
    for style in styles:
        svar = cg.new_Pvariable(style[CONF_ID])
        cgen(f"lv_style_init({svar})")
        for prop, validator in ALL_STYLES.items():
            if value := style.get(prop):
                if isinstance(validator, ty.LValidator):
                    value = await validator.process(value)
                if isinstance(value, list):
                    value = "|".join(value)
                cgen(f"lv_style_set_{prop}({svar}, {value})")


async def theme_to_code(theme):
    helpers.add_lv_use(df.CONF_THEME)
    for w, style in theme.items():
        if not isinstance(style, dict):
            continue

        init = []
        ow = Widget.create(None, "obj", WIDGET_TYPES[w])
        init.extend(await set_obj_properties(ow, style))
        lamb = await cg.process_lambda(
            Lambda(join_lines(init)),
            [(ty.lv_obj_t_ptr, "obj")],
            capture="",
        )
        apply = f"lv_theme_apply_{w}"
        theme_widget_map[w] = apply
        lamb_id = ID(apply, type=ty.lv_lambda_t, is_declaration=True)
        cg.variable(lamb_id, lamb)


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
    defines = [as_macro(m, v) for m, v in lv_defines.items()]
    defines.sort()
    return LV_CONF_H_FORMAT.format("\n".join(defines))


async def msgbox_to_code(conf):
    """
    Construct a message box. This consists of a full-screen translucent background enclosing a centered container
    with an optional title, body, close button and a button matrix. And any other widgets the user cares to add
    :param conf: The config data
    :return: code to add to the init lambda
    """
    helpers.add_lv_use(
        df.TYPE_FLEX, "btnmatrix", df.CONF_BUTTON, df.CONF_LABEL, "MSGBOX", "BTN"
    )
    init = []
    mbid = conf[CONF_ID]
    outer = cg.new_variable(
        ID(mbid.id, is_declaration=True, type=ty.lv_obj_t_ptr), cg.nullptr
    )
    btnm = cg.new_variable(
        ID(f"{mbid.id}_btnm", is_declaration=True, type=ty.lv_obj_t_ptr), cg.nullptr
    )
    msgbox = cg.new_variable(
        ID(f"{mbid.id}_msgbox", is_declaration=True, type=ty.lv_obj_t_ptr), cg.nullptr
    )
    Widget.create(mbid, outer, obj_spec, conf)
    btnm_widg = Widget.create(str(btnm), btnm, btnmatrix_spec, conf)
    text_id, ctrl_list, width_list, _, _ = await get_button_data(
        (conf,), mbid, btnm_widg
    )
    if text := conf.get(df.CONF_BODY):
        text = await lv.lv_text.process(text.get(df.CONF_TEXT))
    if title := conf.get(df.CONF_TITLE):
        title = await lv.lv_text.process(title.get(df.CONF_TEXT))
    close_button = conf[df.CONF_CLOSE_BUTTON]
    init.append(
        f"""{outer} = lv_obj_create(lv_disp_get_layer_top(lv_disp));
                    lv_obj_set_width({outer}, lv_pct(100));
                    lv_obj_set_height({outer}, lv_pct(100));
                    lv_obj_set_style_bg_opa({outer}, 128, 0);
                    lv_obj_set_style_bg_color({outer}, lv_color_black(), 0);
                    lv_obj_set_style_border_width({outer}, 0, 0);
                    lv_obj_set_style_pad_all({outer}, 0, 0);
                    lv_obj_set_style_radius({outer}, 0, 0);
                    lv_obj_add_flag({outer}, LV_OBJ_FLAG_HIDDEN);
                    {msgbox} = lv_msgbox_create({outer}, {title}, {text}, {text_id}, {close_button});
                    lv_obj_set_style_align({msgbox}, LV_ALIGN_CENTER, 0);
                    {btnm} = lv_msgbox_get_btns({msgbox});
                """
    )
    if close_button:
        init.append(
            f"""lv_obj_remove_event_cb(lv_msgbox_get_close_btn({msgbox}), nullptr);
                        lv_obj_add_event_cb(lv_msgbox_get_close_btn({msgbox}), [] (lv_event_t *ev) {{
                            lv_obj_add_flag({outer}, LV_OBJ_FLAG_HIDDEN);
                        }}, LV_EVENT_CLICKED, nullptr);
                    """
        )
    if len(ctrl_list) != 0 or len(width_list) != 0:
        s = f"{msgbox}__tobj"
        init.extend(add_temp_var("lv_obj_t", s))
        init.append(f"{s} = lv_msgbox_get_btns({msgbox})")
        init.extend(set_btn_data(Widget(s, ty.lv_obj_t), ctrl_list, width_list))
    return init


async def keypads_to_code(var, config):
    init = []
    if df.CONF_KEYPADS not in config:
        return init
    helpers.add_lv_use("KEYPAD", "KEY_LISTENER")
    for enc_conf in config[df.CONF_KEYPADS]:
        lpt = enc_conf[df.CONF_LONG_PRESS_TIME].total_milliseconds & 0xFFFF
        lprt = enc_conf[df.CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds & 0xFFFF
        listener = cg.new_Pvariable(
            enc_conf[CONF_ID], ty.lv_indev_type_t.LV_INDEV_TYPE_KEYPAD, lpt, lprt
        )
        await cg.register_parented(listener, var)
        if group := add_group(enc_conf.get(CONF_GROUP)):
            init.append(
                f"lv_indev_set_group(lv_indev_drv_register(&{listener}->drv), {group})"
            )
        else:
            init.append(f"lv_indev_drv_register(&{listener}->drv)")
        for key in df.LV_KEYS.choices:
            if sensor := enc_conf.get(key.lower()):
                b_sensor = await cg.get_variable(sensor)
                kvent = df.LV_KEYS.one_of(key)
                init.append(
                    f"{b_sensor}->add_on_state_callback([](bool state) {{ {listener}->event({kvent}, state); }})",
                )

        return init


async def rotary_encoders_to_code(var, config):
    init = []
    if df.CONF_ROTARY_ENCODERS not in config:
        return init
    helpers.add_lv_use("ROTARY_ENCODER", "KEY_LISTENER")
    for enc_conf in config[df.CONF_ROTARY_ENCODERS]:
        lpt = enc_conf[df.CONF_LONG_PRESS_TIME].total_milliseconds & 0xFFFF
        lprt = enc_conf[df.CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds & 0xFFFF
        listener = cg.new_Pvariable(
            enc_conf[CONF_ID], ty.lv_indev_type_t.LV_INDEV_TYPE_ENCODER, lpt, lprt
        )
        await cg.register_parented(listener, var)
        if group := add_group(enc_conf.get(CONF_GROUP)):
            init.append(
                f"lv_indev_set_group(lv_indev_drv_register(&{listener}->drv), {group})"
            )
        else:
            init.append(f"lv_indev_drv_register(&{listener}->drv)")
        if sensor := enc_conf.get(CONF_SENSOR):
            if isinstance(sensor, dict):
                b_sensor = await cg.get_variable(sensor[df.CONF_LEFT_BUTTON])
                init.append(
                    f"{b_sensor}->add_on_state_callback([](bool state) {{ {listener}->event(LV_KEY_LEFT, state); }})",
                )
                b_sensor = await cg.get_variable(sensor[df.CONF_RIGHT_BUTTON])
                init.append(
                    f"{b_sensor}->add_on_state_callback([](bool state) {{ {listener}->event(LV_KEY_RIGHT, state); }})",
                )
            else:
                sensor = await cg.get_variable(sensor)
                init.append(
                    f"{sensor}->register_listener([](uint32_t count) {{ {listener}->set_count(count); }})",
                )
        b_sensor = await cg.get_variable(enc_conf[df.CONF_ENTER_BUTTON])
        init.append(
            f"{b_sensor}->add_on_state_callback([](bool state) {{ {listener}->event(LV_KEY_ENTER, state); }})",
        )

        return init


async def touchscreens_to_code(var, config):
    init = []
    if df.CONF_TOUCHSCREENS not in config:
        return init
    helpers.add_lv_use("TOUCHSCREEN")
    for touchconf in config[df.CONF_TOUCHSCREENS]:
        touchscreen = await cg.get_variable(touchconf[CONF_TOUCHSCREEN_ID])
        lpt = touchconf[df.CONF_LONG_PRESS_TIME].total_milliseconds & 0xFFFF
        lprt = touchconf[df.CONF_LONG_PRESS_REPEAT_TIME].total_milliseconds & 0xFFFF
        listener = cg.new_Pvariable(touchconf[CONF_ID], lpt, lprt)
        await cg.register_parented(listener, var)
        init.extend(
            [
                f"lv_indev_drv_register(&{listener}->drv)",
                f"{touchscreen}->register_listener({listener})",
            ]
        )
    return init


def add_line_markers(value):
    marks = get_line_marks(value)
    for mark in marks:
        cg.add(cg.RawStatement(mark))


async def generate_triggers(lv_component):
    for w in widget_map.values():
        if w.config:
            init = []
            w_obj = w.obj
            for event, conf in {
                event: conf
                for event, conf in w.config.items()
                if event in df.LV_EVENT_TRIGGERS
            }.items():
                event = df.LV_EVENT[event[3:].upper()]
                conf = conf[0]
                tid = conf[CONF_TRIGGER_ID]
                add_line_markers(tid)
                trigger = cg.new_Pvariable(tid)
                args = w.get_args()
                value = w.get_value()
                await automation.build_automation(trigger, args, conf)
                init.extend(w.add_flag("LV_OBJ_FLAG_CLICKABLE"))
                init.extend(
                    w.set_event_cb(
                        f"{trigger}->trigger({value});", f"LV_EVENT_{event.upper()}"
                    )
                )
            if on_value := w.config.get(CONF_ON_VALUE):
                for conf in on_value:
                    tid = conf[CONF_TRIGGER_ID]
                    add_line_markers(tid)
                    trigger = cg.new_Pvariable(tid)
                    args = w.get_args()
                    value = w.get_value()
                    await automation.build_automation(trigger, args, conf)
                    init.extend(
                        w.set_event_cb(
                            f"{trigger}->trigger({value})",
                            "LV_EVENT_VALUE_CHANGED",
                            f"{lv_component}->get_custom_change_event()",
                        )
                    )
            if align_to := w.config.get(df.CONF_ALIGN_TO):
                target = widget_map[align_to[CONF_ID]].obj
                align = align_to[df.CONF_ALIGN]
                x = align_to[df.CONF_X]
                y = align_to[df.CONF_Y]
                init.append(f"lv_obj_align_to({w_obj}, {target}, {align}, {x}, {y})")
            await add_init_lambda(lv_component, init)


async def disp_update(disp, config: dict):
    init = []
    if bg_color := config.get(df.CONF_DISP_BG_COLOR):
        init.append(
            f"lv_disp_set_bg_color({disp}, {await lv.lv_color.process(bg_color)})"
        )
    if bg_image := config.get(df.CONF_DISP_BG_IMAGE):
        helpers.lvgl_components_required.add("image")
        init.append(f"lv_disp_set_bg_image({disp}, lv_img_from({bg_image}))")
    return init


def get_display_list(config):
    result = []
    display_list = config.get(df.CONF_DISPLAYS)
    if isinstance(display_list, list):
        for display in config.get(df.CONF_DISPLAYS, []):
            result.append(display[CONF_DISPLAY_ID])
    else:
        result.append(display_list)
    return result


def warning_checks(config):
    global_config = CORE.config
    displays = get_display_list(config)
    if display_conf := global_config.get(CONF_DISPLAY):
        for display_id in displays:
            display = list(
                filter(lambda c, k=display_id: c[CONF_ID] == k, display_conf)
            )[0]
            if CONF_LAMBDA in display:
                LOGGER.warning(
                    "Using lambda: in display config not recommended with LVGL"
                )
            if display[CONF_AUTO_CLEAR_ENABLED]:
                LOGGER.warning(
                    "Using auto_clear_enabled: true in display config not recommended with LVGL"
                )
    buffer_frac = config[CONF_BUFFER_SIZE]
    if not CORE.is_host and buffer_frac > 0.5 and "psram" not in global_config:
        LOGGER.warning("buffer_size: may need to be reduced without PSRAM")


async def to_code(config):
    warning_checks(config)
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
    for font in helpers.lv_fonts_used:
        add_define(f"LV_FONT_{font.upper()}")
    add_define("LV_COLOR_DEPTH", config[df.CONF_COLOR_DEPTH])
    default_font = config[df.CONF_DEFAULT_FONT]
    add_define("LV_FONT_DEFAULT", default_font)
    if lv.is_esphome_font(default_font):
        add_define("LV_FONT_CUSTOM_DECLARE", f"LV_FONT_DECLARE(*{default_font})")

    if config[df.CONF_COLOR_DEPTH] == 16:
        add_define(
            "LV_COLOR_16_SWAP",
            "1" if config[df.CONF_BYTE_ORDER] == "big_endian" else "0",
        )
    add_define(
        "LV_COLOR_CHROMA_KEY",
        await lv.lv_color.process(config[df.CONF_TRANSPARENCY_KEY]),
    )
    CORE.add_build_flag("-Isrc")

    cg.add_global(ty.lvgl_ns.using)
    lv_component = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(lv_component, config)
    Widget.create(config[CONF_ID], lv_component, WIDGET_TYPES[df.CONF_OBJ], config)
    displays = get_display_list(config)
    for display in displays:
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
    cgen("lv_init()")
    if df.CONF_ROTARY_ENCODERS in config:  # or df.CONF_KEYBOARDS in config
        cgen("lv_group_set_default(lv_group_create())")
    init = []
    if helpers.esphome_fonts_used:
        for font in helpers.esphome_fonts_used:
            await cg.get_variable(font)
            getter = cg.RawExpression(f"(new lvgl::FontEngine({font}))->get_lv_font()")
            cg.Pvariable(
                ID(f"{font}_as_lv_font_", True, ty.lv_font_t.operator("const")), getter
            )
    if style_defs := config.get(df.CONF_STYLE_DEFINITIONS, []):
        await styles_to_code(style_defs)
    if theme := config.get(df.CONF_THEME):
        await theme_to_code(theme)
    if msgboxes := config.get(df.CONF_MSGBOXES):
        for msgbox in msgboxes:
            init.extend(await msgbox_to_code(msgbox))
    if top_conf := config.get(df.CONF_TOP_LAYER):
        top_layer = Widget(
            "lv_disp_get_layer_top(lv_disp)",
            obj_spec,
            obj="lv_disp_get_layer_top(lv_disp)",
        )
        init.extend(await set_obj_properties(top_layer, top_conf))
        if widgets := top_conf.get(df.CONF_WIDGETS):
            for w in widgets:
                lv_w_type, w_cnfig = next(iter(w.items()))
                ext_init = await widget_to_code(w_cnfig, lv_w_type, top_layer.obj)
                init.extend(ext_init)
    if widgets := config.get(df.CONF_WIDGETS):
        init.extend(await set_obj_properties(lv_scr_act, config))
        for w in widgets:
            lv_w_type, w_cnfig = next(iter(w.items()))
            ext_init = await widget_to_code(w_cnfig, lv_w_type, lv_scr_act.obj)
            init.extend(ext_init)
    if pages := config.get(CONF_PAGES):
        for index, pconf in enumerate(pages):
            pvar, pinit = await page_spec.page_to_code(config, pconf, index)
            init.append(f"{lv_component}->add_page({pvar})")
            init.extend(pinit)

    automation.widgets_completed = True
    init.append(f"{lv_component}->set_page_wrap({config[df.CONF_PAGE_WRAP]})")
    init.extend(await touchscreens_to_code(lv_component, config))
    init.extend(await rotary_encoders_to_code(lv_component, config))
    init.extend(await keypads_to_code(lv_component, config))
    if on_idle := config.get(CONF_ON_IDLE):
        for conf in on_idle:
            templ = await cg.templatable(conf[CONF_TIMEOUT], [], cg.uint32)
            trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], lv_component, templ)
            await automation.build_automation(trigger, [], conf)

    init.extend(await disp_update("lv_disp", config))
    await add_init_lambda(lv_component, init)
    await generate_triggers(lv_component)
    for comp in helpers.lvgl_components_required:
        CORE.add_define(f"LVGL_USES_{comp.upper()}")
    # These must be build flags, since the lvgl code does not read our defines.h
    for use in helpers.lv_uses:
        add_define(f"LV_USE_{use.upper()}")
    lv_conf_h_file = CORE.relative_src_path(LV_CONF_FILENAME)
    write_file_if_changed(lv_conf_h_file, generate_lv_conf_h())
    CORE.add_build_flag("-DLV_CONF_H=1")
    CORE.add_build_flag(f'-DLV_CONF_PATH="{LV_CONF_FILENAME}"')


def indicator_update_schema(base):
    return base.extend({cv.Required(CONF_ID): cv.use_id(ty.lv_meter_indicator_t)})


DISP_BG_SCHEMA = cv.Schema(
    {
        cv.Optional(df.CONF_DISP_BG_IMAGE): cv.use_id(Image_),
        cv.Optional(df.CONF_DISP_BG_COLOR): lv.lv_color,
    }
)

CONFIG_SCHEMA = (
    cv.polling_component_schema("1s")
    .extend(obj_schema("obj"))
    .extend(
        {
            cv.Optional(CONF_ID, default=df.CONF_LVGL_COMPONENT): cv.declare_id(
                ty.LvglComponent
            ),
            cv.GenerateID(df.CONF_DISPLAYS): cv.Any(
                cv.use_id(Display),
                cv.ensure_list(
                    cv.maybe_simple_value(
                        {
                            cv.Required(CONF_DISPLAY_ID): cv.use_id(Display),
                        },
                        key=CONF_DISPLAY_ID,
                    ),
                ),
            ),
            cv.Optional(df.CONF_KEYPADS): cv.ensure_list(
                ENCODER_SCHEMA.extend(
                    {
                        cv.Optional(key.lower()): cv.use_id(BinarySensor)
                        for key in df.LV_KEYS.choices
                    }
                )
            ),
            cv.Optional(df.CONF_TOUCHSCREENS): cv.ensure_list(
                cv.maybe_simple_value(
                    {
                        cv.Required(CONF_TOUCHSCREEN_ID): cv.use_id(Touchscreen),
                        cv.Optional(
                            df.CONF_LONG_PRESS_TIME, default="400ms"
                        ): lv.lv_milliseconds,
                        cv.Optional(
                            df.CONF_LONG_PRESS_REPEAT_TIME, default="100ms"
                        ): lv.lv_milliseconds,
                        cv.GenerateID(): cv.declare_id(ty.LVTouchListener),
                    },
                    key=CONF_TOUCHSCREEN_ID,
                )
            ),
            cv.Optional(df.CONF_ROTARY_ENCODERS): cv.ensure_list(
                ENCODER_SCHEMA.extend(
                    {
                        cv.Required(df.CONF_ENTER_BUTTON): cv.use_id(BinarySensor),
                        cv.Required(CONF_SENSOR): cv.Any(
                            cv.use_id(RotaryEncoderSensor),
                            cv.Schema(
                                {
                                    cv.Optional(df.CONF_LEFT_BUTTON): cv.use_id(
                                        BinarySensor
                                    ),
                                    cv.Optional(df.CONF_RIGHT_BUTTON): cv.use_id(
                                        BinarySensor
                                    ),
                                }
                            ),
                        ),
                    }
                )
            ),
            cv.Optional(df.CONF_COLOR_DEPTH, default=16): cv.one_of(16),
            cv.Optional(df.CONF_DEFAULT_FONT, default="montserrat_14"): lv.font,
            cv.Optional(df.CONF_FULL_REFRESH, default=False): cv.boolean,
            cv.Optional(CONF_BUFFER_SIZE, default="100%"): cv.percentage,
            cv.Optional(df.CONF_LOG_LEVEL, default="WARN"): cv.one_of(
                *df.LOG_LEVELS, upper=True
            ),
            cv.Optional(df.CONF_BYTE_ORDER, default="big_endian"): cv.one_of(
                "big_endian", "little_endian"
            ),
            cv.Optional(df.CONF_STYLE_DEFINITIONS): cv.ensure_list(
                cv.Schema({cv.Required(CONF_ID): cv.declare_id(ty.lv_style_t)})
                .extend(STYLE_SCHEMA)
                .extend(
                    {
                        cv.Optional(df.CONF_GRID_CELL_X_ALIGN): grid_alignments,
                        cv.Optional(df.CONF_GRID_CELL_Y_ALIGN): grid_alignments,
                    }
                )
            ),
            cv.Optional(CONF_ON_IDLE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ty.IdleTrigger),
                    cv.Required(CONF_TIMEOUT): cv.templatable(
                        cv.positive_time_period_milliseconds
                    ),
                }
            ),
            cv.Exclusive(df.CONF_WIDGETS, CONF_PAGES): cv.ensure_list(WIDGET_SCHEMA),
            cv.Exclusive(CONF_PAGES, CONF_PAGES): cv.ensure_list(
                container_schema(df.CONF_PAGE)
            ),
            cv.Optional(df.CONF_MSGBOXES): cv.ensure_list(MSGBOX_SCHEMA),
            cv.Optional(df.CONF_PAGE_WRAP, default=True): lv.lv_bool,
            cv.Optional(df.CONF_TOP_LAYER): container_schema(df.CONF_OBJ),
            cv.Optional(df.CONF_TRANSPARENCY_KEY, default=0x000400): lv.lv_color,
            cv.Optional(df.CONF_THEME): cv.Schema(
                {cv.Optional(w): obj_schema(w) for w in df.WIDGET_PARTS}
            ),
        }
    )
    .extend(DISP_BG_SCHEMA)
).add_extra(cv.has_at_least_one_key(CONF_PAGES, df.CONF_WIDGETS))


def tab_obj_creator(parent: Widget, config: dict):
    return f"lv_tabview_add_tab({parent.obj}, {config[CONF_NAME]})"


@automation.register_action(
    "lvgl.update",
    ty.LvglAction,
    DISP_BG_SCHEMA.extend(
        {
            cv.GenerateID(): cv.use_id(ty.LvglComponent),
        }
    ),
)
async def lvgl_update_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    action = await disp_update("lvgl_comp->get_disp()", config)
    lamb = await cg.process_lambda(
        Lambda(join_lines(action, action_id)),
        [(ty.LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_action(
    "lvgl.widget.redraw",
    ty.ObjUpdateAction,
    cv.Schema(
        {
            cv.Optional(CONF_ID): cv.use_id(ty.lv_obj_t),
            cv.GenerateID(df.CONF_LVGL_ID): cv.use_id(ty.LvglComponent),
        }
    ),
)
async def obj_invalidate_to_code(config, action_id, template_arg, args):
    if obj_id := config.get(CONF_ID):
        w = await get_widget(obj_id)
    else:
        w = lv_scr_act
    action = [f"lv_obj_invalidate({w.obj});"]
    return await action_to_code(action, action_id, w, template_arg, args)


@automation.register_action(
    "lvgl.pause",
    ty.LvglAction,
    {
        cv.GenerateID(): cv.use_id(ty.LvglComponent),
        cv.Optional(df.CONF_SHOW_SNOW, default="false"): lv.lv_bool,
    },
)
async def pause_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    lamb = await cg.process_lambda(
        Lambda(f"lvgl_comp->set_paused(true, {config[df.CONF_SHOW_SNOW]});"),
        [(ty.LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_action(
    "lvgl.resume",
    ty.LvglAction,
    {
        cv.GenerateID(): cv.use_id(ty.LvglComponent),
    },
)
async def resume_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    lamb = await cg.process_lambda(
        Lambda("lvgl_comp->set_paused(false, false);"),
        [(ty.LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_action(lamb))
    return var


@automation.register_condition(
    "lvgl.is_idle",
    ty.LvglCondition,
    LVGL_SCHEMA.extend(
        {
            cv.Required(CONF_TIMEOUT): cv.templatable(
                cv.positive_time_period_milliseconds
            )
        }
    ),
)
async def lvgl_is_idle(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    lvgl = config[df.CONF_LVGL_ID]
    timeout = await cg.templatable(config[CONF_TIMEOUT], [], cg.uint32)
    if isinstance(timeout, LambdaExpression):
        timeout = f"({timeout}())"
    else:
        timeout = timeout.total_milliseconds
    await cg.register_parented(var, lvgl)
    lamb = await cg.process_lambda(
        Lambda(f"return lvgl_comp->is_idle({timeout});"),
        [(ty.LvglComponentPtr, "lvgl_comp")],
    )
    cg.add(var.set_condition_lambda(lamb))
    return var


@automation.register_condition(
    "lvgl.is_paused",
    ty.LvglCondition,
    LVGL_SCHEMA,
)
async def lvgl_is_paused(config, condition_id, template_arg, args):
    var = cg.new_Pvariable(condition_id, template_arg)
    lvgl = config[df.CONF_LVGL_ID]
    await cg.register_parented(var, lvgl)
    lamb = await cg.process_lambda(
        Lambda("return lvgl_comp->is_paused();"), [(ty.LvglComponentPtr, "lvgl_comp")]
    )
    cg.add(var.set_condition_lambda(lamb))
    return var


@automation.register_action("lvgl.widget.disable", ObjUpdateAction, ACTION_SCHEMA)
async def obj_disable_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    w = await get_widget(obj_id)
    action = w.add_state("LV_STATE_DISABLED")
    return await action_to_code(action, action_id, w, template_arg, args)


@automation.register_action("lvgl.widget.enable", ObjUpdateAction, ACTION_SCHEMA)
async def obj_enable_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    w = await get_widget(obj_id)
    action = w.clear_state("LV_STATE_DISABLED")
    return await action_to_code(action, action_id, w, template_arg, args)


@automation.register_action("lvgl.widget.show", ObjUpdateAction, ACTION_SCHEMA)
async def obj_show_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    w = await get_widget(obj_id)
    action = w.clear_flag("LV_OBJ_FLAG_HIDDEN")
    return await action_to_code(action, action_id, w, template_arg, args)


@automation.register_action("lvgl.widget.hide", ObjUpdateAction, ACTION_SCHEMA)
async def obj_hide_to_code(config, action_id, template_arg, args):
    obj_id = config[CONF_ID]
    w = await get_widget(obj_id)
    action = w.add_flag("LV_OBJ_FLAG_HIDDEN")
    return await action_to_code(action, action_id, w, template_arg, args)


@automation.register_action(
    "lvgl.widget.update", ObjUpdateAction, create_modify_schema(df.CONF_OBJ)
)
async def obj_update_to_code(config, action_id, template_arg, args):
    w = await get_widget(config[CONF_ID])
    return await update_to_code(config, action_id, w, [], template_arg, args)
