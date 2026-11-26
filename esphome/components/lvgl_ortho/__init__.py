# SPDX-License-Identifier: MIT
# Copyright (c) 2026 youkorr
# Part of ppa-alternative: https://github.com/youkorr/ppa-alternative-
"""lv_draw_ortho on an ESPHome board: LVGL draws in the panel's orientation.

    lvgl_ortho:
      rotation: 270        # what was on the lvgl: block, which loses it
      ppa: true            # the default: the PPA turns what LVGL draws
      cores: 1             # 2: the second core paints every other tile

Takes LVGL's display over after lvgl has built it: one panel-sized buffer in
LV_DISPLAY_RENDER_MODE_DIRECT, LVGL's matrix rotation drawn by lv_draw_ortho,
and a flush that copies changed rows as they are -- nothing is turned after
it is drawn. A camera is shown with id(...).show_video().
"""

import esphome.codegen as cg
from esphome.coroutine import CoroPriority, coroutine_with_priority
from esphome.components import display
from esphome.components.esp32 import VARIANT_ESP32P4, only_on_variant
import esphome.config_validation as cv
from esphome.const import CONF_DISPLAY_ID, CONF_ID, CONF_ROTATION
import esphome.final_validate as fv

DEPENDENCIES = ["lvgl"]
CODEOWNERS = ["@youkorr"]

lvgl_ortho_ns = cg.esphome_ns.namespace("lvgl_ortho")
LvglOrtho = lvgl_ortho_ns.class_("LvglOrtho", cg.Component)

def _bytes(value):
    """A size in bytes: 4194304, "4MB", "512KB" or 0."""
    if isinstance(value, int):
        return cv.int_range(min=0)(value)
    text = cv.string(value).strip().upper().replace(" ", "")
    for suffix, scale in (("MIB", 1 << 20), ("MB", 1 << 20), ("KIB", 1 << 10), ("KB", 1 << 10), ("B", 1)):
        if text.endswith(suffix):
            number = text[: -len(suffix)]
            break
    else:
        number, scale = text, 1
    try:
        return cv.int_range(min=0)(int(float(number) * scale))
    except ValueError as err:
        raise cv.Invalid(f"a size such as 4MB, 512KB or 0, not {value!r}") from err


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LvglOrtho),
            # ESPHome's degrees, clockwise: the number the lvgl: block had.
            cv.Optional(CONF_ROTATION, default=0): cv.one_of(0, 90, 180, 270, int=True),
            # Left out, the lvgl: block's first display.
            cv.Optional(CONF_DISPLAY_ID): cv.use_id(display.Display),
            # The PPA turns each tile while the CPU draws the next; false
            # leaves the turning to the CPU, to compare the two on a board.
            cv.Optional("ppa", default=True): cv.boolean,
            # Experimental adapter for ESPHome PR #23332, not the legacy fast-present ABI.
            cv.Optional("native_dsi", default=False): cv.one_of(False),
            # Off by default: it takes up to three more tiles of internal RAM
            # and the other core's time. Compare with lvgl_bench first.
            cv.Optional("cores", default=1): cv.int_range(min=1, max=2),
            # PSRAM for pictures turned once: a wallpaper or a background
            # drawn again is then copied, not turned again. Only pictures
            # that cannot change (compiled in, an `image:`); 0 turns it off.
            cv.Optional("image_cache", default="4MB"): _bytes,
            # Internal RAM for turning a big picture in PSRAM -- a camera
            # frame -- in two bands: the GDMA copies a band of its rows into
            # internal RAM while the PPA turns the band before from there,
            # because the PPA reading PSRAM is what makes the turn slow
            # (measured with the camera streaming: 44 to 24 ms on a Tab5 with
            # 80KB, 19 with 160KB; 32 to 14 ms on a Waveshare with 64KB). With
            # 0, or no room at boot, the bands are the halves of the first
            # tile, lent while the picture is turned: no RAM of their own.
            cv.Optional("camera_ram", default="80KB"): _bytes,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    only_on_variant(supported=[VARIANT_ESP32P4]),
)


def _lvgl_blocks(full_config):
    blocks = full_config.get("lvgl") or []
    return blocks if isinstance(blocks, list) else [blocks]


def _the_display(full_config, config):
    if CONF_DISPLAY_ID in config:
        return config[CONF_DISPLAY_ID]
    for block in _lvgl_blocks(full_config):
        displays = block.get("displays") or []
        if displays:
            return displays[0]
    return None


def _final_validate(config):
    full = fv.full_config.get()
    blocks = _lvgl_blocks(full)
    if len(blocks) != 1:
        raise cv.Invalid("lvgl_ortho: needs exactly one lvgl: block")
    turn = int(blocks[0].get(CONF_ROTATION, 0) or 0)
    if turn != 0:
        raise cv.Invalid(
            f"lvgl_ortho: move `rotation: {turn}` from the lvgl: block to "
            f"lvgl_ortho: (rotation: {turn}). The turn is lvgl_ortho's now; "
            "with it on lvgl: as well, every band would be turned twice"
        )
    if _the_display(full, config) is None:
        raise cv.Invalid(f"lvgl_ortho: no display found; give {CONF_DISPLAY_ID}:")
    if config["native_dsi"]:
        target = _the_display(full, config)
        matches = [d for d in full.get("display", []) if d.get(CONF_ID) == target]
        if len(matches) != 1 or matches[0].get("platform") != "mipi_dsi":
            raise cv.Invalid("native_dsi requires a mipi_dsi display")
        if matches[0].get("framebuffers", 1) != 3:
            raise cv.Invalid("native_dsi requires framebuffers: 3")
        if str(matches[0].get("color_depth", "16")).removesuffix("bit") != "16":
            raise cv.Invalid("native_dsi requires color_depth: 16")
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


# LVGL hands every draw task a matrix only with these on, and lv_draw_ortho
# reads it -- the library and this component must be built with them alike.
MATRIX_DEFINES = ("LV_USE_FLOAT", "LV_USE_MATRIX", "LV_DRAW_TRANSFORM_USE_MATRIX")


def _force_matrix_defines():
    """Set the three defines and write lv_conf.h again.

    Done AFTER the lvgl component's own to_code, because that is where it
    decides them: youkorr/lvgl_9.5 sets LV_USE_FLOAT and LV_USE_MATRIX to 0
    whenever no Lottie or SVG widget is used, and writes lv_conf.h on the next
    line, so nothing done earlier survives. Rewriting with lvgl's own
    generator keeps every other define exactly as lvgl chose it.
    """
    from esphome.components import lvgl as lv
    from esphome.components.lvgl import defines as df
    from esphome.core import CORE
    from esphome.helpers import write_file_if_changed

    # ESPHome's lvgl calls it _get_data, youkorr/lvgl_9.5 get_data.
    get = getattr(df, "get_data", None) or getattr(df, "_get_data")
    defines = get(df.KEY_LV_DEFINES, {})
    for name in MATRIX_DEFINES:
        defines[name] = "1"
    write_file_if_changed(CORE.relative_src_path(lv.LV_CONF_FILENAME), lv.generate_lv_conf_h())


@coroutine_with_priority(CoroPriority.FINAL)
async def to_code(config):
    from esphome.core import CORE

    _force_matrix_defines()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    disp = await cg.get_variable(_the_display(CORE.config, config))
    cg.add(var.set_display(disp))
    if config["native_dsi"]:
        cg.add_define("USE_LVGL_ORTHO_NATIVE_DSI")
        cg.add(var.set_native_dsi(disp))
    cg.add(var.set_rotation(config[CONF_ROTATION]))
    cg.add(var.set_ppa(config["ppa"]))
    cg.add(var.set_cores(config["cores"]))
    cg.add(var.set_image_cache(config["image_cache"]))
    cg.add(var.set_camera_ram(config["camera_ram"]))
    cg.add_define("USE_LVGL_ORTHO")
