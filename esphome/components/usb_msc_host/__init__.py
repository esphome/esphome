import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_component,
    only_on_variant,
    require_vfs_dir,
)
from esphome.components.usb_host import CONF_PID, CONF_VID, usb_device_schema
import esphome.config_validation as cv
from esphome.const import CONF_DEVICES, CONF_ID

CODEOWNERS = ["p1ngb4ck"]
DEPENDENCIES = ["usb_host", "esp32"]
AUTO_LOAD = []

CONF_USB_MSC_HOST_ID = "usb_msc_host_id"

require_vfs_dir()

usb_msc_host_ns = cg.esphome_ns.namespace("usb_msc_host")
USBMscHost = usb_msc_host_ns.class_("USBMscHost", cg.Component)


async def register_usb_msc_client(device_config, parent_id):
    var = cg.new_Pvariable(device_config[CONF_ID], device_config[CONF_VID], device_config[CONF_PID])
    await cg.register_component(var, device_config)  # Register as Component for loop() calls
    # Set parent by calling the Parented<USBMscHost>::set_parent() method
    paren = await cg.get_variable(parent_id)
    cg.add(var.set_parent(paren))
    return var


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBMscHost),
            cv.Optional(CONF_DEVICES): cv.ensure_list(usb_device_schema()),
        }
    ),
    cv.only_with_esp_idf,
    only_on_variant(supported=[VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32P4]),
)


async def to_code(config):
    add_idf_component(name="espressif/usb_host_msc", ref="1.1.4")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for device in config.get(CONF_DEVICES) or ():
        await register_usb_msc_client(device, config[CONF_ID])
