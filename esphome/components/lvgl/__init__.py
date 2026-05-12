import importlib
from pathlib import Path
import pkgutil
import re

from esphome.automation import Trigger, build_automation, validate_automation
import esphome.codegen as cg
from esphome.components.const import (
    CONF_BYTE_ORDER,
    CONF_COLOR_DEPTH,
    CONF_DRAW_ROUNDING,
)
from esphome.components.display import Display, get_display_metadata, validate_rotation
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    add_idf_component,
    add_idf_sdkconfig_option,
    get_esp32_variant,
)
from esphome.components.image import (
    CONF_OPAQUE,
    IMAGE_TYPE,
    ImageBinary,
    ImageGrayscale,
    ImageRGB,
    ImageRGB565,
    get_image_metadata,
)
from esphome.components.psram import DOMAIN as PSRAM_DOMAIN
import esphome.config_validation as cv
from esphome.const import (
    CONF_AUTO_CLEAR_ENABLED,
    CONF_BUFFER_SIZE,
    CONF_ESPHOME,
    CONF_GROUP,
    CONF_ID,
    CONF_LAMBDA,
    CONF_LOG_LEVEL,
    CONF_ON_IDLE,
    CONF_PAGES,
    CONF_PLATFORMIO_OPTIONS,
    CONF_ROTATION,
    CONF_TIMEOUT,
    CONF_TRIGGER_ID,
)
from esphome.core import CORE, ID, Lambda
from esphome.cpp_generator import MockObj
from esphome.final_validate import full_config
from esphome.helpers import write_file_if_changed
from esphome.writer import clean_build
from esphome.yaml_util import load_yaml

from . import defines as df, lv_validation as lvalid, widgets
from .automation import layers_to_code, lvgl_update
from .defines import (
    CONF_ALIGN_TO_LAMBDA_ID,
    LOGGER,
    get_focused_widgets,
    get_lv_images_used,
    get_refreshed_widgets,
    set_widgets_completed,
)
from .encoders import (
    ENCODERS_CONFIG,
    encoders_to_code,
    get_default_group,
    initial_focus_to_code,
)
from .gradient import GRADIENT_SCHEMA, gradients_to_code
from .keypads import KEYPADS_CONFIG, keypads_to_code
from .lv_validation import lv_bool
from .lvcode import LvContext, LvglComponent, lv_event_t_ptr, lvgl_static
from .schemas import (
    DISP_BG_SCHEMA,
    FULL_STYLE_SCHEMA,
    STYLE_REMAP,
    WIDGET_TYPES,
    any_widget_schema,
    container_schema,
    obj_schema,
)
from .styles import styles_to_code, theme_to_code
from .touchscreens import touchscreen_schema, touchscreens_to_code
from .trigger import generate_align_tos, generate_triggers
from .types import (
    IdleTrigger,
    PlainTrigger,
    RotationType,
    lv_font_t,
    lv_group_t,
    lv_lambda_t,
    lv_obj_t_ptr,
    lv_style_t,
    lvgl_ns,
)
from .widgets import (
    LvScrActType,
    Widget,
    add_widgets,
    get_screen_active,
    set_obj_properties,
)

# Import only what we actually use directly in this file
from .widgets.msgbox import MSGBOX_SCHEMA, msgboxes_to_code
from .widgets.obj import obj_spec  # Used in LVGL_SCHEMA
from .widgets.page import (  # page_spec used in LVGL_SCHEMA
    add_pages,
    generate_page_triggers,
    page_spec,
)

# Widget registration happens via WidgetType.__init__ in individual widget files
# The imports below trigger creation of the widget types
# Action registration (lvgl.{widget}.update) happens automatically
# in the WidgetType.__init__ method

for module_info in pkgutil.iter_modules(widgets.__path__):
    importlib.import_module(f".widgets.{module_info.name}", package=__package__)

DOMAIN = "lvgl"
DEPENDENCIES = ["display"]
AUTO_LOAD = ["key_provider"]
CODEOWNERS = ["@clydebarrow"]
HELLO_WORLD_FILE = "hello_world.yaml"


SIMPLE_TRIGGERS = (
    df.CONF_ON_PAUSE,
    df.CONF_ON_RESUME,
    df.CONF_ON_DRAW_START,
    df.CONF_ON_DRAW_END,
)


def as_macro(macro, value):
    if value is None:
        return f"#define {macro}"
    return f"#define {macro} {value}"


LVGL_VERSION = "9.5.0"
LV_CONF_FILENAME = "lv_conf.h"
LV_CONF_H_FORMAT = """\
#pragma once
{}
"""


def generate_lv_conf_h():
    # Get all possible LV_ config defines based on the widgets used in the config, and the standard LVGL options
    all_defines = set(
        df.LV_DEFINES + tuple(f"LV_USE_{w.upper()}" for w in WIDGET_TYPES)
    )
    build_flags = (
        CORE.config[CONF_ESPHOME].get(CONF_PLATFORMIO_OPTIONS).get("build_flags", [])
    )
    if not isinstance(build_flags, list):
        build_flags = [build_flags]
    # Extract define names from build flags like '-DLV_USE_CHART=1', '-D LV_USE_CHART',
    # or multiple defines in one string.
    define_pattern = r'-D\s*([A-Z_][A-Z0-9_]*)(?:=[^\s\'"\]]*)?'
    defines_from_flags = {
        m.group(1) for flag in build_flags for m in re.finditer(define_pattern, flag)
    }

    # Get the defines that are actually used based on the config,
    lv_defines = df.get_defines()
    clashes = defines_from_flags & lv_defines.keys()
    if clashes:
        LOGGER.warning(
            "Some defines are set both by ESPHome build flags and by LVGL configuration which may lead to unexpected behavior: %s",
            sorted(list(clashes)),
        )
    unused_defines = all_defines - lv_defines.keys() - defines_from_flags

    # Create the content of lv_conf.h with the used defines set to their value, and the unused defines disabled
    definitions = [as_macro(m, v) for m, v in lv_defines.items()] + [
        as_macro(m, "0") for m in unused_defines
    ]
    definitions.sort()
    return LV_CONF_H_FORMAT.format("\n".join(definitions))


def multi_conf_validate(configs: list[dict]):
    displays = [config[df.CONF_DISPLAYS] for config in configs]
    # flatten the display list
    display_list = [disp for disps in displays for disp in disps]
    if len(display_list) != len(set(display_list)):
        raise cv.Invalid("A display ID may be used in only one LVGL instance")
    for config in configs:
        for item in (df.CONF_ENCODERS, df.CONF_KEYPADS):
            for enc in config.get(item, ()):
                if CONF_GROUP not in enc:
                    raise cv.Invalid(
                        f"'{item}' must have an explicit group set when using multiple LVGL instances"
                    )
    base_config = configs[0]
    for config in configs[1:]:
        for item in (
            CONF_LOG_LEVEL,
            CONF_COLOR_DEPTH,
            CONF_BYTE_ORDER,
            df.CONF_TRANSPARENCY_KEY,
        ):
            if base_config[item] != config[item]:
                raise cv.Invalid(
                    f"Config item '{item}' must be the same for all LVGL instances"
                )


def final_validation(config_list):
    if len(config_list) != 1:
        multi_conf_validate(config_list)
    global_config = full_config.get()
    for config in config_list:
        if (pages := config.get(CONF_PAGES)) and all(p[df.CONF_SKIP] for p in pages):
            raise cv.Invalid("At least one page must not be skipped")
        for display_id in config[df.CONF_DISPLAYS]:
            path = global_config.get_path_for_id(display_id)[:-1]
            display = global_config.get_config_for_path(path)
            if CONF_LAMBDA in display or CONF_PAGES in display:
                raise cv.Invalid(
                    "Using lambda: or pages: in display config is not compatible with LVGL"
                )
            # treating 0 as false is intended here.
            if display.get(CONF_ROTATION):
                raise cv.Invalid(
                    "use of 'rotation' in the display config is not compatible with LVGL, please set rotation in the LVGL config instead"
                )
            if display.get(CONF_AUTO_CLEAR_ENABLED) is True:
                raise cv.Invalid(
                    "Using auto_clear_enabled: true in display config not compatible with LVGL"
                )
            if draw_rounding := display.get(CONF_DRAW_ROUNDING):
                config[CONF_DRAW_ROUNDING] = max(
                    draw_rounding, config[CONF_DRAW_ROUNDING]
                )
        buffer_frac = config[CONF_BUFFER_SIZE]
        if CORE.is_esp32 and buffer_frac > 0.5 and PSRAM_DOMAIN not in global_config:
            df.LOGGER.warning("buffer_size: may need to be reduced without PSRAM")
        for w in get_focused_widgets():
            path = global_config.get_path_for_id(w)
            widget_conf = global_config.get_config_for_path(path[:-1])
            if (
                df.CONF_ADJUSTABLE in widget_conf
                and not widget_conf[df.CONF_ADJUSTABLE]
            ):
                raise cv.Invalid(
                    "A non adjustable arc may not be focused",
                    path,
                )
        for w in get_refreshed_widgets():
            path = global_config.get_path_for_id(w)
            widget_conf = global_config.get_config_for_path(path[:-1])
            if not any(isinstance(v, (Lambda, dict)) for v in widget_conf.values()):
                raise cv.Invalid(
                    f"Widget '{w}' does not have any dynamic properties to refresh",
                )
        # Do per-widget type final validation for update actions
        for widget_type, update_configs in df.get_updated_widgets().items():
            for conf in update_configs:
                for id_conf in conf.get(CONF_ID, ()):
                    name = id_conf[CONF_ID]
                    path = global_config.get_path_for_id(name)
                    widget_conf = global_config.get_config_for_path(path[:-1])
                    widget_type.final_validate(name, conf, widget_conf, path[1:])


async def to_code(configs):
    config_0 = configs[0]
    # Global configuration
    if CORE.is_esp32:
        # Skip compiling lvgl examples
        add_idf_sdkconfig_option("CONFIG_LV_BUILD_EXAMPLES", False)
        add_idf_sdkconfig_option("CONFIG_LV_BUILD_DEMOS", False)
        if get_esp32_variant() == VARIANT_ESP32P4:
            add_idf_sdkconfig_option("CONFIG_LV_DRAW_BUF_ALIGN", 64)
            # disable use of PPA for fills until upstream bugs fixed
            df.add_define("LV_USE_PPA", "0")
            df.add_define("LV_DRAW_BUF_ALIGN", "64")
        else:
            df.add_define("LV_DRAW_BUF_ALIGN", "32")
        add_idf_component(name="lvgl/lvgl", ref=LVGL_VERSION)
    else:
        df.add_define("LV_DRAW_BUF_ALIGN", "1")
        cg.add_library("lvgl/lvgl", LVGL_VERSION)
    df.add_define("LV_DRAW_BUF_STRIDE_ALIGN", "1")
    df.add_define("LV_USE_DRAW_SW", "1")
    df.add_define("LV_USE_STDLIB_SPRINTF", "LV_STDLIB_CLIB")
    df.add_define("LV_USE_STDLIB_STRING", "LV_STDLIB_CLIB")
    df.add_define("LV_USE_STDLIB_MALLOC", "LV_STDLIB_CUSTOM")
    df.add_define("LV_DEF_REFR_PERIOD", "16")
    cg.add_define("USE_LVGL")
    # suppress default enabling of extra widgets
    # cg.add_define("LV_KCONFIG_PRESENT")
    # Always enable - lots of things use it.
    df.add_define("LV_DRAW_SW_COMPLEX", "1")

    df.add_define(
        "LV_LOG_LEVEL",
        f"LV_LOG_LEVEL_{df.LV_LOG_LEVELS[config_0[CONF_LOG_LEVEL]]}",
    )
    df.add_define("LV_USE_LOG", "1")
    cg.add_define(
        "LVGL_LOG_LEVEL",
        cg.RawExpression(f"ESPHOME_LOG_LEVEL_{config_0[CONF_LOG_LEVEL]}"),
    )
    df.add_define("LV_COLOR_DEPTH", config_0[CONF_COLOR_DEPTH])
    for font in df.get_lv_fonts_used():
        df.add_define(f"LV_FONT_{font.upper()}")

    if config_0[CONF_COLOR_DEPTH] == 16:
        df.add_define(
            "LV_COLOR_16_SWAP",
            "1" if config_0[CONF_BYTE_ORDER] == "big_endian" else "0",
        )
    df.add_define(
        "LV_COLOR_CHROMA_KEY",
        await lvalid.lv_color.process(config_0[df.CONF_TRANSPARENCY_KEY]),
    )
    cg.add_build_flag("-Isrc")

    cg.add_global(lvgl_ns.using)
    for font in df.get_esphome_fonts_used():
        await cg.get_variable(font)
    default_font = config_0[df.CONF_DEFAULT_FONT]
    if not lvalid.is_lv_font(default_font):
        df.add_define(
            "LV_FONT_CUSTOM_DECLARE", f"LV_FONT_DECLARE(*{df.DEFAULT_ESPHOME_FONT})"
        )
        globfont_id = ID(
            df.DEFAULT_ESPHOME_FONT,
            True,
            type=lv_font_t.operator("ptr").operator("const"),
        )
        # static=False because LV_FONT_CUSTOM_DECLARE creates an extern declaration
        cg.new_variable(
            globfont_id,
            MockObj(await lvalid.lv_font.process(default_font), "->").get_lv_font(),
            static=False,
        )
        df.add_define("LV_FONT_DEFAULT", df.DEFAULT_ESPHOME_FONT)
    else:
        df.add_define("LV_FONT_DEFAULT", await lvalid.lv_font.process(default_font))
    cg.add(lvgl_static.esphome_lvgl_init())
    default_group = get_default_group(config_0)

    for config in configs:
        frac = config[CONF_BUFFER_SIZE]
        if frac >= 0.75:
            frac = 1
        elif frac >= 0.375:
            frac = 2
        elif frac > 0.19:
            frac = 4
        elif frac != 0:
            frac = 8
        displays = [
            await cg.get_variable(display) for display in config[df.CONF_DISPLAYS]
        ]
        rotation_type = RotationType.ROTATION_UNUSED
        # options will have CONF_ROTATION true if rotation is changed in an automation.
        if CONF_ROTATION in config or df.get_options().get(CONF_ROTATION) is True:
            if all(
                get_display_metadata(str(disp)).has_hardware_rotation
                for disp in displays
            ):
                rotation_type = RotationType.ROTATION_HARDWARE
                df.LOGGER.info("LVGL will use hardware rotation via display driver")
            else:
                rotation_type = RotationType.ROTATION_SOFTWARE
                if CORE.is_esp32 and get_esp32_variant() == VARIANT_ESP32P4:
                    df.LOGGER.info("LVGL will use software rotation (PPA accelerated)")
                else:
                    df.LOGGER.info("LVGL will use software rotation")
        lv_component = cg.new_Pvariable(
            config[CONF_ID],
            displays,
            frac,
            config[df.CONF_FULL_REFRESH],
            config[CONF_DRAW_ROUNDING],
            config[df.CONF_RESUME_ON_INPUT],
            config[df.CONF_UPDATE_WHEN_DISPLAY_IDLE],
            rotation_type,
        )
        await cg.register_component(lv_component, config)
        if rotation := config.get(CONF_ROTATION):
            cg.add(lv_component.set_rotation(rotation))
        Widget.create(config[CONF_ID], lv_component, LvScrActType(), config)

        lv_scr_act = get_screen_active(lv_component)
        async with LvContext():
            cg.add(lv_component.set_big_endian(config[CONF_BYTE_ORDER] == "big_endian"))
            await touchscreens_to_code(lv_component, config)
            await encoders_to_code(lv_component, config, default_group)
            await keypads_to_code(lv_component, config, default_group)
            await theme_to_code(config)
            await gradients_to_code(config)
            await styles_to_code(config)
            await set_obj_properties(lv_scr_act, config)
            await add_widgets(lv_scr_act, config)
            await add_pages(lv_component, config)
            await layers_to_code(lv_component, config)
            await lvgl_update(lv_component, config)
            await msgboxes_to_code(lv_component, config)
            # await disp_update(lv_component.get_disp(), config)
    # Mark all widgets as completed so awaiters of ``wait_for_widgets`` proceed.
    set_widgets_completed(True)
    async with LvContext():
        await generate_triggers()
        await generate_align_tos(configs[0])
        for config in configs:
            lv_component = await cg.get_variable(config[CONF_ID])
            await generate_page_triggers(config)
            await initial_focus_to_code(config)
            for conf in config.get(CONF_ON_IDLE, ()):
                templ = await cg.templatable(conf[CONF_TIMEOUT], [], cg.uint32)
                idle_trigger = cg.new_Pvariable(
                    conf[CONF_TRIGGER_ID], lv_component, templ
                )
                await build_automation(idle_trigger, [], conf)
            for trigger_name in SIMPLE_TRIGGERS:
                if conf := config.get(trigger_name):
                    trigger_var = cg.new_Pvariable(conf[CONF_TRIGGER_ID])
                    await build_automation(trigger_var, [], conf)
                    cg.add(
                        getattr(
                            lv_component,
                            f"set_{trigger_name.removeprefix('on_')}_trigger",
                        )(trigger_var)
                    )

    # This must be done after all widgets are created
    styles_used = df.get_styles_used()
    for use in df.get_lv_uses():
        df.add_define(f"LV_USE_{use.upper()}")
        cg.add_define(f"USE_LVGL_{use.upper()}")

    if {
        "transform_rotation",
        "transform_scale",
        "transform_scale_x",
        "transform_scale_y",
    } & styles_used:
        df.add_define("LV_COLOR_SCREEN_TRANSP", "1")

    if configs[0].get(df.CONF_THEME, {}).get(df.CONF_DARK_MODE):
        df.add_define("LV_THEME_DEFAULT_DARK", "1")

    # Currently always need RGB565 for the display buffer, and ARGB8888 is used for layer blending
    lv_image_formats = {"RGB565", "ARGB8888"}
    if {
        "drop_shadow_color",
        "drop_shadow_offset_x",
        "drop_shadow_offset_y",
        "drop_shadow_opa",
        "drop_shadow_quality",
        "drop_shadow_radius",
    } & styles_used:
        lv_image_formats.add("A8")

    for image_id in get_lv_images_used():
        await cg.get_variable(image_id)
        metadata = get_image_metadata(image_id.id)
        image_type = IMAGE_TYPE[metadata.image_type]
        transparent = metadata.transparency != CONF_OPAQUE
        if image_type == ImageBinary:
            lv_image_formats.add("I1")
        if image_type == ImageGrayscale:
            lv_image_formats.add("A8")
        if image_type == ImageRGB565:
            lv_image_formats.add("RGB565A8" if transparent else "RGB565")
        if image_type == ImageRGB:
            lv_image_formats.add("ARGB8888" if transparent else "RGB8888")
    if df.is_defined("LV_GRADIENT_MAX_STOPS"):
        lv_image_formats.add("RGB888")
    for fmt in lv_image_formats:
        df.add_define(f"LV_DRAW_SW_SUPPORT_{fmt}", "1")

    lv_conf_h_file = CORE.relative_src_path(LV_CONF_FILENAME)
    if write_file_if_changed(lv_conf_h_file, generate_lv_conf_h()):
        clean_build(clear_pio_cache=False)
    cg.add_build_flag("-DLV_CONF_H=1")
    # handle windows paths in a way that doesn't break the generated C++
    lv_conf_h_path = Path(lv_conf_h_file).as_posix()
    cg.add_build_flag(f'-DLV_CONF_PATH=\\"{lv_conf_h_path}\\"')
    cg.add_build_flag("-DLV_KCONFIG_IGNORE")

    for prop in df.get_remapped_uses():
        df.LOGGER.warning(
            "Property '%s' is deprecated, use '%s' instead", prop, STYLE_REMAP[prop]
        )
    for warning in df.get_warnings():
        df.LOGGER.warning(warning)


def display_schema(config):
    value = cv.ensure_list(cv.use_id(Display))(config)
    value = value or [cv.use_id(Display)(config)]
    if len(set(value)) != len(value):
        raise cv.Invalid("Display IDs must be unique")
    return value


def add_hello_world(config):
    if df.CONF_WIDGETS not in config and CONF_PAGES not in config:
        df.LOGGER.info(
            "No pages or widgets configured, creating default hello_world page"
        )
        hello_world_path = Path(__file__).parent / HELLO_WORLD_FILE
        config[df.CONF_WIDGETS] = any_widget_schema()(load_yaml(hello_world_path))
    return config


def _theme_schema(value):
    return cv.Schema(
        {
            cv.Optional(df.CONF_DARK_MODE, default=False): cv.boolean,
            **{
                cv.Optional(name): obj_schema(w).extend(FULL_STYLE_SCHEMA)
                for name, w in WIDGET_TYPES.items()
            },
        }
    )(value)


FINAL_VALIDATE_SCHEMA = final_validation

LVGL_SCHEMA = cv.All(
    container_schema(
        obj_spec,
        cv.polling_component_schema("1s")
        .extend(
            {
                **{
                    cv.Optional(event): validate_automation(
                        {
                            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                                Trigger.template(lv_obj_t_ptr, lv_event_t_ptr)
                            ),
                        }
                    )
                    for event in df.LV_SCREEN_EVENT_TRIGGERS
                    + df.LV_DISPLAY_EVENT_TRIGGERS
                },
                cv.GenerateID(CONF_ID): cv.declare_id(LvglComponent),
                cv.GenerateID(CONF_ALIGN_TO_LAMBDA_ID): cv.declare_id(lv_lambda_t),
                cv.GenerateID(df.CONF_DISPLAYS): display_schema,
                cv.Optional(CONF_COLOR_DEPTH, default=16): cv.one_of(16),
                cv.Optional(
                    df.CONF_DEFAULT_FONT, default="montserrat_14"
                ): lvalid.lv_font,
                cv.Optional(df.CONF_FULL_REFRESH, default=False): cv.boolean,
                cv.Optional(
                    df.CONF_UPDATE_WHEN_DISPLAY_IDLE, default=False
                ): cv.boolean,
                cv.Optional(CONF_DRAW_ROUNDING, default=2): cv.positive_int,
                cv.Optional(CONF_BUFFER_SIZE, default=0): cv.percentage,
                cv.Optional(CONF_ROTATION): validate_rotation,
                cv.Optional(CONF_LOG_LEVEL, default="WARN"): cv.one_of(
                    *df.LV_LOG_LEVELS, upper=True
                ),
                cv.Optional(CONF_BYTE_ORDER, default="big_endian"): cv.one_of(
                    "big_endian", "little_endian", lower=True
                ),
                cv.Optional(df.CONF_STYLE_DEFINITIONS): cv.ensure_list(
                    cv.Schema({cv.Required(CONF_ID): cv.declare_id(lv_style_t)}).extend(
                        FULL_STYLE_SCHEMA
                    )
                ),
                cv.Optional(CONF_ON_IDLE): validate_automation(
                    {
                        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(IdleTrigger),
                        cv.Required(CONF_TIMEOUT): cv.templatable(
                            cv.positive_time_period_milliseconds
                        ),
                    }
                ),
                cv.Optional(CONF_PAGES): cv.ensure_list(container_schema(page_spec)),
                **{
                    cv.Optional(x): validate_automation(
                        {
                            cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(PlainTrigger),
                        },
                        single=True,
                    )
                    for x in SIMPLE_TRIGGERS
                },
                cv.Optional(df.CONF_MSGBOXES): cv.ensure_list(MSGBOX_SCHEMA),
                cv.Optional(df.CONF_PAGE_WRAP, default=True): lv_bool,
                cv.Optional(df.CONF_TOP_LAYER): container_schema(obj_spec),
                cv.Optional(df.CONF_BOTTOM_LAYER): container_schema(obj_spec),
                cv.Optional(
                    df.CONF_TRANSPARENCY_KEY, default=0x000400
                ): lvalid.lv_color,
                cv.Optional(df.CONF_THEME): _theme_schema,
                cv.Optional(df.CONF_GRADIENTS): GRADIENT_SCHEMA,
                cv.Optional(df.CONF_TOUCHSCREENS, default=None): touchscreen_schema,
                cv.Optional(df.CONF_ENCODERS, default=None): ENCODERS_CONFIG,
                cv.Optional(df.CONF_KEYPADS, default=None): KEYPADS_CONFIG,
                cv.GenerateID(df.CONF_DEFAULT_GROUP): cv.declare_id(lv_group_t),
                cv.Optional(df.CONF_RESUME_ON_INPUT, default=True): cv.boolean,
            }
        )
        .extend(DISP_BG_SCHEMA),
    ),
    cv.has_at_most_one_key(CONF_PAGES, df.CONF_LAYOUT),
    add_hello_world,
)


def lvgl_config_schema(config):
    """
    Can't use cv.ensure_list here because it converts an empty config to an empty list,
    rather than a default config.
    """
    if not config or isinstance(config, dict):
        return [LVGL_SCHEMA(config)]
    return cv.Schema([LVGL_SCHEMA])(config)


CONFIG_SCHEMA = lvgl_config_schema
