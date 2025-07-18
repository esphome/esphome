import esphome.codegen as cg
from esphome.components import binary_sensor, key_provider
from esphome.components.const import CONF_ENABLE_HUBS, CONF_PID, CONF_VID
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    add_idf_component,
    add_idf_sdkconfig_option,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["key_provider"]

CODEOWNERS = ["@zopieux"]

CONFLICTS_WITH = ["usb_host"]

DEPENDENCIES = ["esp32"]

usb_host_hid_ns = cg.esphome_ns.namespace("usb_host_hid")
USBHidHost = usb_host_hid_ns.class_("USBHidHost", cg.Component)
USBHidKeyboard = usb_host_hid_ns.class_(
    "USBHidKeyboard",
    cg.Component,
    binary_sensor.BinarySensorInitiallyOff,
    key_provider.KeyProvider,
)

CONF_CHECK_CLASS = "check_class"
CONF_KEYBOARDS = "keyboards"


def usb_hid_device_schema(
    cls=USBHidKeyboard, vid: int = None, pid: [int] = None, check_class: bool = True
) -> cv.Schema:
    schema = cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(cls),
            cv.Optional(CONF_CHECK_CLASS, default=True): cv.boolean,
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
            cv.GenerateID(): cv.declare_id(USBHidHost),
            cv.Optional(CONF_ENABLE_HUBS, default=False): cv.boolean,
            cv.Optional(CONF_KEYBOARDS): cv.ensure_list(usb_hid_device_schema()),
        }
    ),
    cv.only_with_esp_idf,
    only_on_variant(supported=[VARIANT_ESP32S2, VARIANT_ESP32S3, VARIANT_ESP32P4]),
)


async def register_usb_hid_device(config):
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_VID], config[CONF_PID])
    await cg.register_component(var, config)
    return var


async def to_code(config):
    add_idf_component(name="espressif/usb_host_hid", ref="1.0.1")
    add_idf_sdkconfig_option("CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE", 1024)
    if config.get(CONF_ENABLE_HUBS):
        add_idf_sdkconfig_option("CONFIG_USB_HOST_HUBS_SUPPORTED", True)
    host_var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(host_var, config)
    for kbd in config.get(CONF_KEYBOARDS) or ():
        dev_var = await register_usb_hid_device(kbd)
        cg.add(host_var.register_keyboard(dev_var, kbd[CONF_CHECK_CLASS]))
