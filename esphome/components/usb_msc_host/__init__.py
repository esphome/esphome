import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_component,
    only_on_variant,
    require_vfs_dir,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["p1ngb4ck"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = []

require_vfs_dir()

usb_msc_host_ns = cg.esphome_ns.namespace("usb_msc_host")
USBMscHost = usb_msc_host_ns.class_("USBMscHost", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBMscHost),
        }
    ),
    cv.only_with_esp_idf,
    only_on_variant(supported=[VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32P4]),
)


async def to_code(config):
    add_idf_component(name="espressif/usb_host_msc", ref="1.1.4")
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
