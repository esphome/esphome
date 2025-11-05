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
CONF_RETRY_ENABLED = "retry_enabled"
CONF_RETRY_INTERVAL = "retry_interval"
CONF_RETRY_MAX_ATTEMPTS = "retry_max_attempts"
CONF_USE_HARDWARE_DECODER = "use_hardware_decoder"

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
        # NEW: Retry mechanism configuration (Ansatz 2)
        cv.Optional(CONF_RETRY_ENABLED, default=True): cv.boolean,
        cv.Optional(
            CONF_RETRY_INTERVAL, default="2s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RETRY_MAX_ATTEMPTS, default=30): cv.int_range(min=1, max=100),
        # Hardware JPEG decoder (ESP32-P4 only)
        cv.Optional(CONF_USE_HARDWARE_DECODER, default=True): cv.boolean,
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
    from esphome.components.esp32 import add_idf_component, get_esp32_variant
    from esphome.components.esp32.const import VARIANT_ESP32P4

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add_define("USE_STORAGE_HOST")

    # Add hardware JPEG decoder support for ESP32-P4
    if get_esp32_variant() == VARIANT_ESP32P4:
        add_idf_component("esp_driver_jpeg")
        cg.add_define("USE_HARDWARE_JPEG_DECODER")
        _LOGGER.info("Hardware JPEG decoder enabled for ESP32-P4")

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
    file_path = config[CONF_FILE]
    cg.add(var.set_file_path(file_path))

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
                        "Invalid resize format: %s. Use WIDTHxHEIGHT", resize_str
                    )

    # NEW: Configure retry mechanism (Ansatz 2)
    cg.add(var.set_retry_enabled(config[CONF_RETRY_ENABLED]))
    cg.add(var.set_retry_interval(config[CONF_RETRY_INTERVAL]))
    cg.add(var.set_retry_max_attempts(config[CONF_RETRY_MAX_ATTEMPTS]))

    # Hardware decoder configuration
    cg.add(var.set_use_hardware_decoder(config[CONF_USE_HARDWARE_DECODER]))

    # NEW: Event-based loading (Ansatz 3)
    # Register callback with USB MSC devices if available
    from esphome.core import CORE

    if hasattr(CORE, "data") and "usb_msc_devices" in CORE.data:
        for usb_device in CORE.data["usb_msc_devices"]:
            # Register callback: when USB device is mounted, try to load this image
            # Create a lambda that captures the StorageImage pointer by value
            cg.add(
                usb_device.add_mount_ready_callback(
                    cg.RawExpression(
                        f"[=](const std::string &mount_path) {{ {var}->on_mount_ready(mount_path); }}"
                    )
                )
            )
            _LOGGER.info(
                "Registered mount ready callback for image '%s' with USB MSC device",
                file_path,
            )

    # Register callback with SD MMC devices if available
    if hasattr(CORE, "data") and "sd_mmc_devices" in CORE.data:
        for sd_device in CORE.data["sd_mmc_devices"]:
            # Register callback: when SD card is mounted, try to load this image
            # Create a lambda that captures the StorageImage pointer by value
            cg.add(
                sd_device.add_mount_ready_callback(
                    cg.RawExpression(
                        f"[=](const std::string &mount_path) {{ {var}->on_mount_ready(mount_path); }}"
                    )
                )
            )
            _LOGGER.info(
                "Registered mount ready callback for image '%s' with SD MMC device",
                file_path,
            )

    return var
