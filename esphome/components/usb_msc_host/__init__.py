import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_component,
    only_on_variant,
    require_vfs_dir,
)
from esphome.components.usb_host import USBHost, usb_host_ns
import esphome.config_validation as cv
from esphome.const import CONF_DEVICES, CONF_ID

CODEOWNERS = ["p1ngb4ck"]
DEPENDENCIES = ["usb_host", "esp32"]
AUTO_LOAD = []

CONF_USB_HOST_ID = "usb_host_id"

require_vfs_dir()

usb_msc_host_ns = cg.esphome_ns.namespace("usb_msc_host")
USBMscHost = usb_msc_host_ns.class_("USBMscHost", cg.Component)
USBMscDevice = usb_msc_host_ns.class_(
    "USBMscDevice",
    cg.Component,
    usb_host_ns.class_("USBDeviceHandler"),
    cg.Parented.template(USBMscHost),
)


async def register_usb_msc_handler(device_config, msc_host, usb_host):
    # NEW: Interface-class based handler (no VID/PID needed)
    var = cg.new_Pvariable(device_config[CONF_ID])
    await cg.register_component(var, device_config)
    cg.add(var.set_parent(msc_host))  # Set USBMscHost as parent
    cg.add(usb_host.register_device_handler(var))  # Register as interface-class handler with USBHost
    return var


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBMscHost),
            cv.GenerateID(CONF_USB_HOST_ID): cv.use_id(usb_host.USBHost),  # NEW: Reference to USBHost
            cv.Optional(CONF_DEVICES): cv.ensure_list(
                cv.COMPONENT_SCHEMA.extend({cv.GenerateID(): cv.declare_id(USBMscDevice)})
            ),
        }
    ),
    cv.only_with_esp_idf,
    only_on_variant(supported=[VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32P4]),
)


async def to_code(config):
    add_idf_component(name="espressif/usb_host_msc", ref="1.1.4")

    # Get USBHost instance for handler registration
    usb_host_var = await cg.get_variable(config[CONF_USB_HOST_ID])

    # Create USBMscHost
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Register interface-class based handlers
    for device in config.get(CONF_DEVICES) or ():
        await register_usb_msc_handler(device, var, usb_host_var)
