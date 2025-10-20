import math

from esphome import pins
import esphome.codegen as cg
from esphome.components import canbus
from esphome.components.canbus import CONF_BIT_RATE, CanbusComponent, CanSpeed
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_RX_PIN,
    CONF_RX_QUEUE_LEN,
    CONF_TX_PIN,
    CONF_TX_QUEUE_LEN,
)

CODEOWNERS = ["@Sympatron"]
DEPENDENCIES = ["esp32"]

CONF_TX_ENQUEUE_TIMEOUT = "tx_enqueue_timeout"
CONF_ADVANCED_BIT_RATE = "advanced_bit_rate"
CONF_PRESCALER = "prescaler"
CONF_TSEG_1 = "tseg_1"
CONF_TSEG_2 = "tseg_2"

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

def validate_prescaler(value):
    if value <= 0:
        raise cv.Invalid("Prescaler needs to be a positive integer")
    if value <= 128 and value % 2 != 0:
        raise cv.Invalid("Prescaler needs to be an even number if less or equal to 128")
    if value >= 132 and value % 4 != 0:
        raise cv.Invalid("Prescaler needs to be a multiple of 4 if more or equal to 132")
    return value

CONFIG_SCHEMA = canbus.CANBUS_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(esp32_can),
        cv.Optional(CONF_BIT_RATE, default="125KBPS"): validate_bit_rate,
        cv.Required(CONF_RX_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_TX_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_RX_QUEUE_LEN): cv.uint32_t,
        cv.Optional(CONF_TX_QUEUE_LEN): cv.uint32_t,
        cv.Optional(CONF_TX_ENQUEUE_TIMEOUT): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ADVANCED_BIT_RATE):
            {
                cv.Required(CONF_PRESCALER): validate_prescaler,
                cv.Required(CONF_TSEG_1): cv.int_range(1,16),
                cv.Required(CONF_TSEG_2): cv.int_range(1,8),
            },
    }
)


def get_default_tx_enqueue_timeout(bit_rate_config):
    if CONF_BRP in bit_rate_config:
        bit_rate_numeric = 80_000_000 / (bit_rate_config[CONF_PRESCALER] * (1 + bit_rate_config[CONF_TSEG_1] + bit_rate_config[CONF_TSEG_2]))
    else:
        bit_rate_numeric = canbus.get_rate(bit_rate_config)
    bits_per_packet = 140  # ~max CAN message length
    ms_per_packet = bits_per_packet / bit_rate_numeric * 1000
    return int(
        max(min(math.ceil(10 * ms_per_packet), 1000), 1)
    )  # ~10 packet lengths, min 1ms, max 1000ms

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await canbus.register_canbus(var, config)

    cg.add(var.set_rx(config[CONF_RX_PIN]))
    cg.add(var.set_tx(config[CONF_TX_PIN]))
    if (rx_queue_len := config.get(CONF_RX_QUEUE_LEN)) is not None:
        cg.add(var.set_rx_queue_len(rx_queue_len))
    if (tx_queue_len := config.get(CONF_TX_QUEUE_LEN)) is not None:
        cg.add(var.set_tx_queue_len(tx_queue_len))

    if CONF_TX_ENQUEUE_TIMEOUT in config:
        tx_enqueue_timeout_ms = config[CONF_TX_ENQUEUE_TIMEOUT].total_milliseconds
    elif CONF_ADVANCED_BIT_RATE in config:
        tx_enqueue_timeout_ms = get_default_tx_enqueue_timeout(config[CONF_ADVANCED_BIT_RATE])
    else:
        tx_enqueue_timeout_ms = get_default_tx_enqueue_timeout(config[CONF_BIT_RATE])

    cg.add(var.set_tx_enqueue_timeout_ms(tx_enqueue_timeout_ms))
    if CONF_ADVANCED_BIT_RATE in config:
        cg.add(var.set_brp(config[CONF_ADVANCED_BIT_RATE][CONF_PRESCALER]))
        cg.add(var.set_tseg1(config[CONF_ADVANCED_BIT_RATE][CONF_TSEG_1]))
        cg.add(var.set_tseg2(config[CONF_ADVANCED_BIT_RATE][CONF_TSEG_2]))
