import esphome.codegen as cg
from esphome.components.esp32.const import KEY_ESP32, KEY_VARIANT
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_sdkconfig_option,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEVICES, CONF_ID
from esphome.core import CORE
from esphome.cpp_types import Component
from esphome.types import ConfigType

AUTO_LOAD = ["bytebuffer"]
CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["esp32"]
usb_host_ns = cg.esphome_ns.namespace("usb_host")
USBHost = usb_host_ns.class_("USBHost", Component)
USBClient = usb_host_ns.class_("USBClient", Component)

CONF_VID = "vid"
CONF_PID = "pid"
CONF_ENABLE_HUBS = "enable_hubs"
CONF_MAX_TRANSFER_REQUESTS = "max_transfer_requests"
CONF_USE_HIGH_SPEED = "use_high_speed"


def usb_device_schema(cls=USBClient, vid: int = None, pid: [int] = None) -> cv.Schema:
    schema = cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(cls),
        }
    )
    if vid:
        schema = schema.extend({cv.Optional(CONF_VID, default=vid): cv.hex_uint16_t})
    else:
        schema = schema.extend({cv.Required(CONF_VID): cv.hex_uint16_t})
    if pid:
        schema = schema.extend({cv.Optional(CONF_PID, default=pid): cv.hex_uint16_t})
    else:
        schema = schema.extend({cv.Required(CONF_PID): cv.hex_uint16_t})
    return schema


def get_target_variant():
    """Get the target ESP32 variant from CORE.data."""
    return CORE.data.get(KEY_ESP32, {}).get(KEY_VARIANT, "")


def get_mps(use_high_speed=None):
    variant = get_target_variant()
    if variant == VARIANT_ESP32P4:
        return 512 if use_high_speed else 64
    return 64


def validate_use_high_speed(config):
    """Validate use_high_speed option - only allowed on ESP32-P4."""
    if config.get(CONF_USE_HIGH_SPEED, False):
        variant = get_target_variant()
        if "P4" not in variant:
            raise cv.Invalid(
                f"'use_high_speed' option is only supported on ESP32-P4. "
                f"Current variant: {variant or 'ESP32-S2/S3'}"
            )
    return config


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBHost),
            cv.Optional(CONF_ENABLE_HUBS, default=False): cv.boolean,
            cv.Optional(CONF_MAX_TRANSFER_REQUESTS, default=16): cv.int_range(
                min=1, max=32
            ),
            cv.Optional(CONF_USE_HIGH_SPEED, default=True): cv.boolean,
            cv.Optional(CONF_DEVICES): cv.ensure_list(usb_device_schema()),
        }
    ),
    cv.only_with_esp_idf,
    only_on_variant(supported=[VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32P4]),
    validate_use_high_speed,
)


async def register_usb_client(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_VID], config[CONF_PID])
    await cg.register_component(var, config)
    return var


async def to_code(config: ConfigType) -> None:
    add_idf_sdkconfig_option("CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE", 1024)
    if config.get(CONF_ENABLE_HUBS):
        add_idf_sdkconfig_option("CONFIG_USB_HOST_HUBS_SUPPORTED", True)

    max_requests = config[CONF_MAX_TRANSFER_REQUESTS]
    cg.add_define("USB_HOST_MAX_REQUESTS", max_requests)
    use_high_speed = device.get(CONF_USE_HIGH_SPEED, True)
    mps = get_mps(use_high_speed)
    cg.add_define("USB_MAX_PACKET_SIZE", mps)
    
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for device in config.get(CONF_DEVICES) or ():
        await register_usb_client(device)
