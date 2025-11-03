import esphome.codegen as cg
from esphome.components import socket
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_sdkconfig_option,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEVICES, CONF_ID
from esphome.cpp_types import Component
from esphome.types import ConfigType

AUTO_LOAD = ["bytebuffer", "socket"]
CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["esp32"]
usb_host_ns = cg.esphome_ns.namespace("usb_host")
USBHost = usb_host_ns.class_("USBHost", Component)
USBClient = usb_host_ns.class_("USBClient", Component)

CONF_VID = "vid"
CONF_PID = "pid"
CONF_ENABLE_HUBS = "enable_hubs"
CONF_MAX_TRANSFER_REQUESTS = "max_transfer_requests"


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


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBHost),
            cv.Optional(CONF_ENABLE_HUBS, default=False): cv.boolean,
            cv.Optional(CONF_MAX_TRANSFER_REQUESTS, default=16): cv.int_range(
                min=1, max=32
            ),
            cv.Optional(CONF_DEVICES): cv.ensure_list(usb_device_schema()),
        }
    ),
    cv.only_with_esp_idf,
    only_on_variant(supported=[VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32P4]),
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

    # USB uses the socket wake_loop_threadsafe() mechanism to wake the main loop from USB task
    # This enables low-latency (~12μs) USB event processing instead of waiting for
    # select() timeout (0-16ms). The wake socket is shared across all components.
    socket.require_wake_loop_threadsafe()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for device in config.get(CONF_DEVICES) or ():
        await register_usb_client(device)
