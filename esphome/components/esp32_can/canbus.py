import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import canbus
from esphome.const import CONF_ID, CONF_RX_PIN, CONF_TX_PIN
from esphome.components.canbus import CanbusComponent, CanSpeed, CONF_BIT_RATE

from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
)

CODEOWNERS = ["@Sympatron"]
DEPENDENCIES = ["esp32"]

esp32_can_ns = cg.esphome_ns.namespace("esp32_can")
esp32_can = esp32_can_ns.class_("ESP32Can", CanbusComponent)

# Currently the driver only supports a subset of the bit rates defined in canbus
# The supported bit rates differ between ESP32 variants.
# See ESP-IDF Programming Guide --> API Reference --> Two-Wire Automotive Interface (TWAI)

CAN_SPEEDS_ESP32 = {
    "25KBPS": CanSpeed.CAN_25KBPS,
    "50KBPS": CanSpeed.CAN_50KBPS,
    "100KBPS": CanSpeed.CAN_100KBPS,
    "125KBPS": CanSpeed.CAN_125KBPS,
    "250KBPS": CanSpeed.CAN_250KBPS,
    "500KBPS": CanSpeed.CAN_500KBPS,
    "800KBPS": CanSpeed.CAN_800KBPS,
    "1000KBPS": CanSpeed.CAN_1000KBPS,
}

CAN_SPEEDS_ESP32_S2 = {
    "1KBPS": CanSpeed.CAN_1KBPS,
    "5KBPS": CanSpeed.CAN_5KBPS,
    "10KBPS": CanSpeed.CAN_10KBPS,
    "12K5BPS": CanSpeed.CAN_12K5BPS,
    "16KBPS": CanSpeed.CAN_16KBPS,
    "20KBPS": CanSpeed.CAN_20KBPS,
    **CAN_SPEEDS_ESP32,
}

CAN_SPEEDS_ESP32_S3 = {**CAN_SPEEDS_ESP32_S2}
CAN_SPEEDS_ESP32_C3 = {**CAN_SPEEDS_ESP32_S2}
CAN_SPEEDS_ESP32_C6 = {**CAN_SPEEDS_ESP32_S2}
CAN_SPEEDS_ESP32_H2 = {**CAN_SPEEDS_ESP32_S2}

CAN_SPEEDS = {
    VARIANT_ESP32: CAN_SPEEDS_ESP32,
    VARIANT_ESP32S2: CAN_SPEEDS_ESP32_S2,
    VARIANT_ESP32S3: CAN_SPEEDS_ESP32_S3,
    VARIANT_ESP32C3: CAN_SPEEDS_ESP32_C3,
    VARIANT_ESP32C6: CAN_SPEEDS_ESP32_C6,
    VARIANT_ESP32H2: CAN_SPEEDS_ESP32_H2,
}


def validate_bit_rate(value):
    variant = get_esp32_variant()
    if variant not in CAN_SPEEDS:
        raise cv.Invalid(f"{variant} is not supported by component {esp32_can_ns}")
    value = value.upper()
    if value not in CAN_SPEEDS[variant]:
        raise cv.Invalid(f"Bit rate {value} is not supported on {variant}")
    return cv.enum(CAN_SPEEDS[variant])(value)


CONFIG_SCHEMA = canbus.CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(esp32_can),
        cv.Optional(CONF_BIT_RATE, default="125KBPS"): validate_bit_rate,
        cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_number,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await canbus.register_canbus(var, config)

    cg.add(var.set_rx(config[CONF_RX_PIN]))
    cg.add(var.set_tx(config[CONF_TX_PIN]))
