from __future__ import annotations

import logging

import esphome.codegen as cg
from esphome import automation
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)

_LOGGER = logging.getLogger(__name__)

# Import LVGL canvas type for proper widget ID validation
try:
    from esphome.components.lvgl.widgets.canvas import lv_canvas_t
    LVGL_AVAILABLE = True
except ImportError:
    LVGL_AVAILABLE = False
    lv_canvas_t = None

# Register codec requirements with transcoder
from esphome.components.transcoder import require_jpeg_decoder

require_jpeg_decoder()  # Picture viewer only needs JPEG decoder, not encoder

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = []
AUTO_LOAD = ["transcoder"]

# Namespaces
picture_viewer_ns = cg.esphome_ns.namespace("picture_viewer")

# Classes
PictureViewer = picture_viewer_ns.class_("PictureViewer", cg.Component)

# Enums
SlideshowMode = picture_viewer_ns.enum("SlideshowMode", is_class=True)
ImageFitMode = picture_viewer_ns.enum("ImageFitMode", is_class=True)
IMAGE_FIT_MODES = {
    "SCALE_TO_FIT": ImageFitMode.SCALE_TO_FIT,
    "SCALE_TO_FILL": ImageFitMode.SCALE_TO_FILL,
    "STRETCH": ImageFitMode.STRETCH,
    "CENTER": ImageFitMode.CENTER,
}

# JPEG decoder configuration enums
JPEG_RGB_ORDER = {
    "RGB": 0,  # Big endian
    "BGR": 1,  # Little endian
}

JPEG_COLOR_SPACE = {
    "BT601": 0,
    "BT709": 1,
}

JPEG_OUTPUT_FORMAT = {
    "RGB888": 0x02000000,  # COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB888)
    "RGB565": 0x02000002,  # COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565)
    "GRAY": 0x03000000,    # COLOR_TYPE_ID(COLOR_SPACE_GRAY, COLOR_PIXEL_GRAY8)
}

# Configuration keys
CONF_FILE_MANAGER_ID = "file_manager_id"
CONF_CANVAS_ID = "canvas_id"
CONF_DISPLAY_ID = "display_id"
CONF_DIRECTORIES = "directories"
CONF_PATH = "path"
CONF_SLIDESHOW_INTERVAL = "slideshow_interval"
CONF_ENABLE_THUMBNAILS = "enable_thumbnails"
CONF_THUMBNAIL_WIDTH = "thumbnail_width"
CONF_THUMBNAIL_HEIGHT = "thumbnail_height"
CONF_FIT_MODE = "fit_mode"
CONF_JPEG_RGB_ORDER = "jpeg_rgb_order"
CONF_JPEG_COLOR_SPACE = "jpeg_color_space"
CONF_JPEG_OUTPUT_FORMAT = "jpeg_output_format"

# Directory configuration schema
DIRECTORY_SCHEMA = cv.Schema({
    cv.Required(CONF_PATH): cv.string,
    cv.Optional(CONF_JPEG_RGB_ORDER, default="BGR"): cv.enum(JPEG_RGB_ORDER, upper=True),
    cv.Optional(CONF_JPEG_COLOR_SPACE, default="BT601"): cv.enum(JPEG_COLOR_SPACE, upper=True),
    cv.Optional(CONF_JPEG_OUTPUT_FORMAT, default="RGB565"): cv.enum(JPEG_OUTPUT_FORMAT, upper=True),
})

# Component configuration
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PictureViewer),
        cv.Optional(CONF_FILE_MANAGER_ID): cv.use_id(cg.Component),  # FileManager
        cv.Optional(CONF_CANVAS_ID): cv.use_id(lv_canvas_t) if LVGL_AVAILABLE else cv.invalid("LVGL not available"),  # LVGL Canvas widget
        cv.Optional(CONF_DISPLAY_ID): cv.use_id(cg.Component),  # Display
        cv.Required(CONF_DIRECTORIES): cv.All(cv.ensure_list(DIRECTORY_SCHEMA), cv.Length(min=1)),
        cv.Optional(
            CONF_SLIDESHOW_INTERVAL, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ENABLE_THUMBNAILS, default=True): cv.boolean,
        cv.Optional(CONF_THUMBNAIL_WIDTH, default=120): cv.int_range(min=32, max=320),
        cv.Optional(CONF_THUMBNAIL_HEIGHT, default=90): cv.int_range(min=32, max=240),
        cv.Optional(CONF_FIT_MODE, default="SCALE_TO_FIT"): cv.enum(
            IMAGE_FIT_MODES, upper=True
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    """Generate code for picture_viewer component"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Link file manager if provided
    if CONF_FILE_MANAGER_ID in config:
        fm = await cg.get_variable(config[CONF_FILE_MANAGER_ID])
        cg.add(var.set_file_manager(fm))

    # Link LVGL canvas if provided
    if CONF_CANVAS_ID in config:
        canvas = await cg.get_variable(config[CONF_CANVAS_ID])
        # Store canvas ID for debugging
        cg.add(var.set_canvas_id(str(config[CONF_CANVAS_ID])))
        # Set the canvas object pointer (canvas is already lv_obj_t*)
        cg.add(var.set_canvas(canvas))

    # Link display if provided
    if CONF_DISPLAY_ID in config:
        display = await cg.get_variable(config[CONF_DISPLAY_ID])
        cg.add(var.set_display(display))

    # Configuration
    cg.add(var.set_slideshow_interval(config[CONF_SLIDESHOW_INTERVAL]))
    cg.add(var.set_enable_thumbnails(config[CONF_ENABLE_THUMBNAILS]))
    cg.add(var.set_thumbnail_width(config[CONF_THUMBNAIL_WIDTH]))
    cg.add(var.set_thumbnail_height(config[CONF_THUMBNAIL_HEIGHT]))
    cg.add(var.set_fit_mode(config[CONF_FIT_MODE]))

    # Add directories with per-directory JPEG decoder configuration
    for dir_config in config[CONF_DIRECTORIES]:
        cg.add(
            var.add_directory(
                dir_config[CONF_PATH],
                dir_config[CONF_JPEG_RGB_ORDER],
                dir_config[CONF_JPEG_COLOR_SPACE],
                dir_config[CONF_JPEG_OUTPUT_FORMAT],
            )
        )

    # Link to transcoder component (handles all decoder initialization)
    # The transcoder dependency ensures it's initialized before picture_viewer
    cg.add(var.set_transcoder(cg.RawExpression("esphome::transcoder::global_transcoder")))

    _LOGGER.info("Picture viewer configured to use transcoder component")

    return var
