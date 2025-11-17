"""Motion Map Component for ESPHome.

This component uses Wi-Fi Channel State Information (CSI) to detect motion
without cameras or microphones, providing privacy-preserving presence detection.
"""
import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32S3,
    add_idf_sdkconfig_option,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["esp32", "wifi"]
AUTO_LOAD = []

motion_map_ns = cg.esphome_ns.namespace("motion_map")
MotionMapComponent = motion_map_ns.class_("MotionMapComponent", cg.Component)

# For sub-components to reference the parent
CONF_MOTION_MAP_ID = "motion_map_id"

# Configuration keys
CONF_MOTION_THRESHOLD = "motion_threshold"
CONF_IDLE_THRESHOLD = "idle_threshold"
CONF_WINDOW_SIZE = "window_size"
CONF_MAC_ADDRESS = "mac_address"
CONF_SENSITIVITY = "sensitivity"

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(MotionMapComponent),
            cv.Optional(CONF_MOTION_THRESHOLD, default=0.5): cv.float_range(
                min=0.0, max=1.0
            ),
            cv.Optional(CONF_IDLE_THRESHOLD, default=0.2): cv.float_range(
                min=0.0, max=1.0
            ),
            cv.Optional(CONF_WINDOW_SIZE, default=100): cv.int_range(
                min=10, max=500
            ),
            cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
            cv.Optional(CONF_SENSITIVITY, default=1.0): cv.float_range(
                min=0.1, max=5.0
            ),
        }
    ),
    only_on_variant(supported=[VARIANT_ESP32S3]),
)


async def to_code(config):
    """Generate C++ code for the motion map component."""
    # Enable CSI in ESP-IDF SDK config
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_ENABLE_CSI", True)
    add_idf_sdkconfig_option("CONFIG_ESP_WIFI_CSI_ENABLED", True)

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_motion_threshold(config[CONF_MOTION_THRESHOLD]))
    cg.add(var.set_idle_threshold(config[CONF_IDLE_THRESHOLD]))
    cg.add(var.set_window_size(config[CONF_WINDOW_SIZE]))
    cg.add(var.set_sensitivity(config[CONF_SENSITIVITY]))

    if CONF_MAC_ADDRESS in config:
        mac_address = config[CONF_MAC_ADDRESS].parts
        cg.add(
            var.set_mac_address(
                [
                    mac_address[0],
                    mac_address[1],
                    mac_address[2],
                    mac_address[3],
                    mac_address[4],
                    mac_address[5],
                ]
            )
        )

    # Add ESP-IDF component dependencies
    if CORE.using_esp_idf:
        cg.add_library("esp_wifi", None)
