from __future__ import annotations

import logging

import esphome.codegen as cg
from esphome import automation
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
)

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = []
AUTO_LOAD = []

# Namespaces
picture_viewer_ns = cg.esphome_ns.namespace("picture_viewer")

# Classes
PictureViewer = picture_viewer_ns.class_("PictureViewer", cg.Component)

# Enums
SlideshowMode = picture_viewer_ns.enum("SlideshowMode")

# Configuration keys
CONF_FILE_MANAGER_ID = "file_manager_id"
CONF_CANVAS_ID = "canvas_id"
CONF_DISPLAY_ID = "display_id"
CONF_WATCH_DIRECTORY = "watch_directory"
CONF_SLIDESHOW_INTERVAL = "slideshow_interval"
CONF_ENABLE_THUMBNAILS = "enable_thumbnails"
CONF_THUMBNAIL_WIDTH = "thumbnail_width"
CONF_THUMBNAIL_HEIGHT = "thumbnail_height"

# Component configuration
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(PictureViewer),
        cv.Optional(CONF_FILE_MANAGER_ID): cv.use_id(cg.Component),  # FileManager
        cv.Optional(CONF_CANVAS_ID): cv.string,  # LVGL Canvas - widget ID as string
        cv.Optional(CONF_DISPLAY_ID): cv.use_id(cg.Component),  # Display
        cv.Required(CONF_WATCH_DIRECTORY): cv.string,
        cv.Optional(
            CONF_SLIDESHOW_INTERVAL, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ENABLE_THUMBNAILS, default=True): cv.boolean,
        cv.Optional(CONF_THUMBNAIL_WIDTH, default=120): cv.int_range(min=32, max=320),
        cv.Optional(CONF_THUMBNAIL_HEIGHT, default=90): cv.int_range(min=32, max=240),
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

    # Link LVGL canvas if provided (pass as string - LVGL uses its own ID system)
    if CONF_CANVAS_ID in config:
        cg.add(var.set_canvas_id(config[CONF_CANVAS_ID]))

    # Link display if provided
    if CONF_DISPLAY_ID in config:
        display = await cg.get_variable(config[CONF_DISPLAY_ID])
        cg.add(var.set_display(display))

    # Configuration
    cg.add(var.set_watch_directory(config[CONF_WATCH_DIRECTORY]))
    cg.add(var.set_slideshow_interval(config[CONF_SLIDESHOW_INTERVAL]))
    cg.add(var.set_enable_thumbnails(config[CONF_ENABLE_THUMBNAILS]))
    cg.add(var.set_thumbnail_width(config[CONF_THUMBNAIL_WIDTH]))
    cg.add(var.set_thumbnail_height(config[CONF_THUMBNAIL_HEIGHT]))

    # Add defines based on platform
    from esphome.components.esp32 import get_esp32_variant, add_idf_component

    variant = get_esp32_variant()
    variant_lower = variant.lower() if variant else ""

    _LOGGER.info("Detected ESP32 variant: %s", variant)

    if variant_lower in ("esp32s2", "esp32-s2"):
        # Enable esp_jpeg decoder for S2/S3
        # Add esp_jpeg as a managed ESP-IDF component from ESP Component Registry
        add_idf_component(name="espressif/esp_jpeg", ref="1.3.1")
        cg.add_define("USE_ESP_JPEG_DECODER")
        _LOGGER.info("Enabled esp_jpeg decoder v1.3.1 for %s", variant)
    elif variant_lower in ("esp32s3", "esp32-s3"):
        # Enable esp_jpeg decoder for S2/S3
        # Add esp_jpeg as a managed ESP-IDF component from ESP Component Registry
        add_idf_component(name="espressif/esp_jpeg", ref="1.3.1")
        cg.add_define("USE_ESP_JPEG_DECODER")
        _LOGGER.info("Enabled esp_jpeg decoder v1.3.1 for %s", variant)
    elif variant_lower in ("esp32p4", "esp32-p4"):
        # Enable hardware JPEG decoder for P4
        cg.add_define("USE_HARDWARE_JPEG_DECODER")
        _LOGGER.info("Enabled hardware JPEG decoder for %s", variant)
    else:
        # Use JPEGDec library as fallback
        cg.add_library("bodmer/JPEGDecoder", "1.8.0")
        cg.add_define("USE_JPEGDEC")
        _LOGGER.info("Using JPEGDec library for %s", variant)

    return var
