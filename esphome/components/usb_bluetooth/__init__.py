"""A USB Bluetooth dongle (HCI over USB) as a usb_host client."""

import esphome.codegen as cg
from esphome.components.const import CONF_FIRMWARE, CONF_MANUFACTURER
from esphome.components.esp32 import (
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    only_on_variant,
)
from esphome.components.usb_host import (
    CONF_INTERFACE_DESCRIPTION,
    CONF_MAX_TRANSFER_REQUESTS,
    CONF_PID,
    CONF_PRODUCT,
    CONF_VID,
    USBClient,
    get_max_packet_size,
    register_usb_client,
    validate_usb_clients,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import realtek

DOMAIN = "usb_bluetooth"
CODEOWNERS = ["@martijnbuts"]
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["usb_host"]
MULTI_CONF = True

CONF_FIRMWARE_CONFIG = "firmware_config"

# A Realtek download fragment is an HCI command of 3 + 1 + 252 bytes, sent as one control
# transfer behind the 8 byte setup packet: more than the 256 byte transfer buffers hold.
FIRMWARE_MIN_PACKET_SIZE = 512
# The event read stays queued all the time, and is queued again from its own callback before
# that one's slot is free; the next command can be sent before the last one's slot is free.
MIN_TRANSFER_REQUESTS = 4

usb_bluetooth_ns = cg.esphome_ns.namespace("usb_bluetooth")
USBBluetooth = usb_bluetooth_ns.class_("USBBluetooth", USBClient)
RealtekImage = USBBluetooth.class_("RealtekImage")


def _read_firmware(config: ConfigType) -> tuple[dict[int, bytes], int]:
    fw = config[CONF_FIRMWARE].read_bytes()
    cfg = (
        config[CONF_FIRMWARE_CONFIG].read_bytes()
        if CONF_FIRMWARE_CONFIG in config
        else None
    )
    return realtek.images(fw, cfg)


def _validate_firmware(config: ConfigType) -> ConfigType:
    if CONF_FIRMWARE_CONFIG in config and CONF_FIRMWARE not in config:
        raise cv.Invalid(f"{CONF_FIRMWARE_CONFIG} needs {CONF_FIRMWARE}")
    if CONF_FIRMWARE in config:
        try:
            _read_firmware(config)
        except realtek.NotFirmware as err:
            raise cv.Invalid(f"{CONF_FIRMWARE}: {err}") from err
    return config


# vid/pid default to 0 (any device): the client then takes the first device with a Bluetooth
# controller interface (class E0, subclass 01, protocol 01) and leaves every other device alone.
CONFIG_SCHEMA = cv.All(
    cv.COMPONENT_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(USBBluetooth),
            cv.Optional(CONF_VID, default=0): cv.hex_uint16_t,
            cv.Optional(CONF_PID, default=0): cv.hex_uint16_t,
            cv.Optional(CONF_MANUFACTURER): cv.string_strict,
            cv.Optional(CONF_PRODUCT): cv.string_strict,
            cv.Optional(CONF_INTERFACE_DESCRIPTION): cv.string_strict,
            # Realtek firmware: linux-firmware rtl_bt/<chip>_fw.bin and, where the chip has one,
            # <chip>_config.bin. Downloaded into a Realtek controller still running its ROM.
            cv.Optional(CONF_FIRMWARE): cv.file_,
            cv.Optional(CONF_FIRMWARE_CONFIG): cv.file_,
        }
    ),
    _validate_firmware,
    only_on_variant(
        supported=[
            VARIANT_ESP32H4,
            VARIANT_ESP32P4,
            VARIANT_ESP32S2,
            VARIANT_ESP32S3,
            VARIANT_ESP32S31,
        ]
    ),
)


def _final_validate(config: ConfigType) -> ConfigType:
    if CONF_FIRMWARE in config and get_max_packet_size() < FIRMWARE_MIN_PACKET_SIZE:
        raise cv.Invalid(
            f"Downloading Realtek firmware sends HCI commands of 256 bytes in one control "
            f"transfer: set usb_host max_packet_size to {FIRMWARE_MIN_PACKET_SIZE} or more"
        )
    full = fv.full_config.get()
    instances = full.get(DOMAIN) or []
    # Checked once, for the first entry: two entries that could match the same device would race for it.
    # Other usb_host clients may match the same device: this client only takes a device's Bluetooth
    # controller interface and leaves devices without one alone.
    if instances and config is instances[0]:
        validate_usb_clients(instances)
    usb_host_config = full.get("usb_host") or {}
    if (
        usb_host_config.get(CONF_MAX_TRANSFER_REQUESTS, MIN_TRANSFER_REQUESTS)
        < MIN_TRANSFER_REQUESTS
    ):
        raise cv.Invalid(
            f"usb_bluetooth needs usb_host {CONF_MAX_TRANSFER_REQUESTS} of {MIN_TRANSFER_REQUESTS} or more"
        )
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    var = await register_usb_client(config)
    if CONF_FIRMWARE in config:
        images, lmp_subver = _read_firmware(config)
        cg.add(
            var.set_realtek_firmware_storage(
                cg.static_storage(config[CONF_ID], RealtekImage, len(images))
            )
        )
        for rom, data in sorted(images.items()):
            # Identical firmware in several entries is stored once
            arr = cg.shared_progmem_array(
                f"{config[CONF_ID].id}_rtl_rom{rom}", cg.uint8, list(data)
            )
            cg.add(var.add_realtek_firmware(lmp_subver, rom, arr, len(data)))
