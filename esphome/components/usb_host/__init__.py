import itertools
import logging

import esphome.codegen as cg
from esphome.components.const import CONF_MANUFACTURER
from esphome.components.esp32 import (
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    add_idf_component,
    add_idf_sdkconfig_option,
    idf_version,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_DEVICES, CONF_ID
from esphome.core import CORE
from esphome.cpp_generator import MockObj
from esphome.cpp_types import Component
import esphome.final_validate as fv
from esphome.helpers import cpp_u16string_escape
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["bytebuffer"]
CODEOWNERS = ["@clydebarrow"]
DEPENDENCIES = ["esp32"]
usb_host_ns = cg.esphome_ns.namespace("usb_host")
USBHost = usb_host_ns.class_("USBHost", Component)
USBClient = usb_host_ns.class_("USBClient", Component)
DOMAIN = "usb_host"
CONF_VID = "vid"
CONF_PID = "pid"
CONF_PRODUCT = "product"
CONF_ENABLE_HUBS = "enable_hubs"
CONF_MAX_TRANSFER_REQUESTS = "max_transfer_requests"
CONF_MAX_PACKET_SIZE = "max_packet_size"


# VID/PID set to 0 or `None` product/manufacturer are wildcards
_FILTER_WILDCARDS = {
    CONF_VID: 0,
    CONF_PID: 0,
    CONF_MANUFACTURER: None,
    CONF_PRODUCT: None,
}


def usb_device_schema(
    cls=USBClient, vid: int | None = None, pid: int | None = None
) -> cv.Schema:
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

    return schema.extend(
        {
            cv.Optional(CONF_MANUFACTURER): cv.string_strict,
            cv.Optional(CONF_PRODUCT): cv.string_strict,
        }
    )


def _clients_overlap(first: ConfigType, second: ConfigType) -> bool:
    # Two entries overlap when no field they both constrain tells them apart
    for key, wildcard in _FILTER_WILDCARDS.items():
        a = first.get(key)
        b = second.get(key)
        if wildcard not in (a, b) and a != b:
            return False
    return True


def validate_usb_clients(configs: list[ConfigType]) -> list[ConfigType]:
    for first, second in itertools.combinations(configs, 2):
        if _clients_overlap(first, second):
            raise cv.Invalid(
                f"USB configs overlap: {first[CONF_ID]}, {second[CONF_ID]}"
            )
    return configs


def _final_validate(config: ConfigType) -> ConfigType:
    # Any two overlapping clients could otherwise open the same device
    devices = config.get(CONF_DEVICES) or []
    uarts = fv.full_config.get().get("usb_uart") or []
    validate_usb_clients(devices)
    validate_usb_clients(uarts)
    # Remove before 2027.10.0, then pool devices and uarts in one validate_usb_clients call
    for device, uart in itertools.product(devices, uarts):
        if not _clients_overlap(device, uart):
            continue
        # The device matches nothing the usb_uart entry does not already claim
        redundant = all(
            uart.get(key) in (wildcard, device.get(key))
            for key, wildcard in _FILTER_WILDCARDS.items()
        )
        _LOGGER.warning(
            "usb_host device '%s' matches the same USB device as usb_uart '%s'; %s. "
            "This will be an error in 2027.10.0",
            device[CONF_ID],
            uart[CONF_ID],
            "remove it from usb_host devices"
            if redundant
            else "narrow its filter so it no longer matches that device",
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


def _set_max_packet_size(config: dict) -> dict:
    CORE.data.setdefault(DOMAIN, {})[CONF_MAX_PACKET_SIZE] = config[
        CONF_MAX_PACKET_SIZE
    ]
    return config


def get_max_packet_size() -> int:
    return CORE.data.get(DOMAIN, {}).get(CONF_MAX_PACKET_SIZE, 64)


CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBHost),
            cv.Optional(CONF_ENABLE_HUBS, default=False): cv.boolean,
            cv.Optional(CONF_MAX_TRANSFER_REQUESTS, default=16): cv.int_range(
                min=1, max=32
            ),
            cv.Optional(CONF_MAX_PACKET_SIZE, default=64): cv.one_of(
                64, 128, 256, 512, 1024, int=True
            ),
            cv.Optional(CONF_DEVICES): cv.ensure_list(usb_device_schema()),
        }
    ),
    only_on_variant(
        supported=[
            VARIANT_ESP32H4,
            VARIANT_ESP32P4,
            VARIANT_ESP32S2,
            VARIANT_ESP32S3,
            VARIANT_ESP32S31,
        ]
    ),
    _set_max_packet_size,
)


async def register_usb_client(config: ConfigType) -> MockObj:
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_VID], config[CONF_PID])
    await cg.register_component(var, config)
    # UTF-16 literals, the encoding the descriptors use, so the device compares code units
    if (manufacturer := config.get(CONF_MANUFACTURER)) is not None:
        cg.add(
            var.set_manufacturer_filter(
                cg.RawExpression(cpp_u16string_escape(manufacturer))
            )
        )
    if (product := config.get(CONF_PRODUCT)) is not None:
        cg.add(var.set_product_filter(cg.RawExpression(cpp_u16string_escape(product))))
    return var


async def to_code(config: ConfigType) -> None:
    # IDF 6.0 moved USB host to an external component
    if idf_version() >= cv.Version(6, 0, 0):
        add_idf_component(name="espressif/usb", ref="1.4.1")
    add_idf_sdkconfig_option("CONFIG_USB_HOST_CONTROL_TRANSFER_MAX_SIZE", 1024)
    if config.get(CONF_ENABLE_HUBS):
        add_idf_sdkconfig_option("CONFIG_USB_HOST_HUBS_SUPPORTED", True)

    cg.add_define("USB_HOST_MAX_REQUESTS", config[CONF_MAX_TRANSFER_REQUESTS])
    cg.add_define("USB_HOST_MAX_PACKET_SIZE", config[CONF_MAX_PACKET_SIZE])

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    for device in config.get(CONF_DEVICES) or ():
        await register_usb_client(device)
