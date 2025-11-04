from __future__ import annotations

import logging

import esphome.codegen as cg
from esphome.components import image
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TYPE

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["image"]
AUTO_LOAD = []

# Namespaces
storage_host_ns = cg.esphome_ns.namespace("storage_host")

# Classes
StorageHost = storage_host_ns.class_("StorageHost", cg.Component)
StorageImage = storage_host_ns.class_("StorageImage", cg.Component, image.Image_)

# Configuration keys
CONF_MOUNTS = "mounts"
CONF_MOUNT_ID = "id"
CONF_MOUNT_PATH = "path"
CONF_MOUNT_PLATFORM = "platform"
CONF_STORAGE_IMAGES = "storage_images"
CONF_FILE = "file"
CONF_FILE_PATH = "file_path"
CONF_FORMAT = "format"
CONF_AUTO_LOAD = "auto_load"
CONF_MOUNT_SOURCE = "mount_source"
CONF_RESIZE = "resize"
CONF_BYTE_ORDER = "byte_order"

PLATFORM_SD_DIRECT = "sd_direct"
PLATFORM_USB_MSC = "usb_msc"
PLATFORM_SD_MMC = "sd_mmc"
PLATFORM_SPIFFS = "spiffs"

# Single mount configuration
MOUNT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MOUNT_ID): cv.declare_id(cg.void),
        cv.Required(CONF_MOUNT_PATH): cv.string,
        cv.Optional(CONF_MOUNT_PLATFORM, default=PLATFORM_SD_DIRECT): cv.one_of(
            PLATFORM_SD_DIRECT, PLATFORM_USB_MSC, PLATFORM_SD_MMC, PLATFORM_SPIFFS
        ),
    }
)

# Storage image configuration - loads images from mounted storage
STORAGE_IMAGE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StorageImage),
        cv.Required(CONF_FILE): cv.string,
        cv.Optional(CONF_FORMAT, default="RGB565"): cv.one_of(
            "RGB565", "RGB888", "RGBA"
        ),
        cv.Optional(CONF_TYPE, default="RGB565"): cv.one_of("RGB565", "RGB888", "RGBA"),
        cv.Optional(CONF_AUTO_LOAD, default=True): cv.boolean,
        cv.Optional(CONF_MOUNT_SOURCE): cv.string,
        cv.Optional(CONF_RESIZE): cv.string,
        cv.Optional(CONF_BYTE_ORDER, default="little_endian"): cv.one_of(
            "little_endian", "big_endian"
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(StorageHost),
        cv.Optional(CONF_MOUNTS, default=[]): cv.ensure_list(MOUNT_SCHEMA),
        cv.Optional(CONF_STORAGE_IMAGES, default=[]): cv.ensure_list(
            STORAGE_IMAGE_SCHEMA
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_define("USE_STORAGE_HOST")

    # JPEGDEC library can be added via include_libs in YAML config

    # Register each mount
    for mount_config in config.get(CONF_MOUNTS, []):
        mount_path = mount_config[CONF_MOUNT_PATH]
        mount_platform = mount_config[CONF_MOUNT_PLATFORM]

        cg.add(var.register_mount(mount_path, mount_platform))

    # Register storage images - following WebDAVBox3 pattern
    if CONF_STORAGE_IMAGES in config:
        for img_config in config[CONF_STORAGE_IMAGES]:
            await setup_storage_image_component(img_config, var)


async def setup_storage_image_component(config, parent_storage):
    """Configure a StorageImage component following WebDAVBox3 pattern"""
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Link to parent storage_host component
    cg.add(var.set_storage_host(parent_storage))

    # Set file path
    cg.add(var.set_file_path(config[CONF_FILE]))

    # Get string values from config - pass as strings directly
    format_str = config[CONF_FORMAT]
    cg.add(var.set_format(format_str))

    # Configure auto_load
    cg.add(var.set_auto_load(config[CONF_AUTO_LOAD]))

    # Set mount source if provided
    if CONF_MOUNT_SOURCE in config:
        cg.add(var.set_mount_source(config[CONF_MOUNT_SOURCE]))

    # Handle resize if provided (WIDTHxHEIGHT format)
    if CONF_RESIZE in config:
        resize_str = config[CONF_RESIZE]
        if "x" in resize_str.lower():
            parts = resize_str.lower().split("x")
            if len(parts) == 2:
                try:
                    width = int(parts[0].strip())
                    height = int(parts[1].strip())
                    cg.add(var.set_resize(width, height))
                except ValueError:
                    _LOGGER.error(
                        f"Invalid resize format: {resize_str}. Use WIDTHxHEIGHT"
                    )

    return var
