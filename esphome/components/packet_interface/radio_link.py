"""Shared code generation for the packet interface links over packet radios, such as the SX126x and SX127x."""

from collections.abc import Callable
import math

import esphome.codegen as cg
from esphome.components.const import CONF_MAX_PACKET_SIZE
import esphome.config_validation as cv
from esphome.const import CONF_TIMEOUT
from esphome.core import CORE
from esphome.cpp_generator import MockObjClass
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import new_packet_interface, packet_interface_schema, reliability_layers
from .layers import CONF_ADDRESSING, CONF_PEERS, STACK_ORDER
from .layers.hdlc_reliability import CONF_ACK_DELAY, DEFAULT_TIMEOUT

# The radios' payload length register is 8 bits
MAX_PACKET_SIZE = 255

# Listen before talk holds a send back for at most the airtime of the largest packet, and never less than this
MIN_MAX_BUSY_MS = 2000
# Room for the preamble detection and the radio's own delays
MARGIN = 1.25
# The radio drivers stop waiting for a packet to be sent after this long
TRANSMIT_TIMEOUT_MS = 4000
# An acknowledgement, with room for the layers below the reliability layer, such as addressing and the rolling code
ACK_PACKET_SIZE = 32


def packet_airtime_ms(radio: ConfigType, size: int) -> float:
    """The time a radio configured as in radio takes to send a packet of size bytes.

    LoRa follows the Semtech formula for an explicit header; the low data rate optimization is assumed when a symbol
    takes more than 16 ms, as the drivers enable it. Other modulations count the preamble, sync word, length byte,
    payload and CRC at the bitrate, treating the preamble size as bytes, which gives the longer time.
    """
    if str(radio["modulation"]) == "LORA":
        spreading_factor = radio["spreading_factor"]
        bandwidth_hz = (
            float(str(radio["bandwidth"]).removesuffix("kHz").replace("_", ".")) * 1000
        )
        coding_rate = int(str(radio["coding_rate"])[-1]) - 4
        symbol_s = (1 << spreading_factor) / bandwidth_hz
        low_data_rate = 1 if symbol_s > 0.016 else 0
        crc = 16 if radio.get("crc_enable", False) else 0
        payload_symbols = 8 + max(
            math.ceil(
                (8 * size - 4 * spreading_factor + 28 + crc)
                / (4 * (spreading_factor - 2 * low_data_rate))
            )
            * (coding_rate + 4),
            0,
        )
        return (radio["preamble_size"] + 4.25 + payload_symbols) * symbol_s * 1000
    bytes_sent = (
        radio["preamble_size"] + len(radio.get("sync_value", [])) + 1 + size + 2
    )
    return bytes_sent * 8 / radio["bitrate"] * 1000


def max_busy_ms(radio: ConfigType, max_packet_size: int) -> int:
    """How long listen before talk may hold a send back, for packets of up to max_packet_size bytes."""
    return max(
        MIN_MAX_BUSY_MS, math.ceil(packet_airtime_ms(radio, max_packet_size) * MARGIN)
    )


def reliability_timeout_ms(
    radio: ConfigType, max_packet_size: int, ack_delay_ms: int
) -> int:
    """The shortest reliability timeout: before acknowledging, the peer may be sending its own largest packet."""
    airtime = packet_airtime_ms(radio, max_packet_size) + packet_airtime_ms(
        radio, ACK_PACKET_SIZE
    )
    return math.ceil(airtime * MARGIN) + ack_delay_ms


def _check_transmit_time(config: ConfigType, radio: ConfigType) -> None:
    """The radio drivers give up on a packet that takes longer to send than they wait."""
    size = config[CONF_MAX_PACKET_SIZE]

    def fits(length: int) -> bool:
        return packet_airtime_ms(radio, length) * MARGIN <= TRANSMIT_TIMEOUT_MS

    if fits(size):
        return
    largest = next((length for length in range(size - 1, 0, -1) if fits(length)), None)
    advice = (
        f"set '{CONF_MAX_PACKET_SIZE}' to {largest} or less"
        if largest is not None
        else "use faster radio settings"
    )
    raise cv.Invalid(
        f"With these radio settings a packet of {size} bytes takes {packet_airtime_ms(radio, size):.0f} ms to "
        f"send, longer than the radio waits for; {advice}",
        path=[CONF_MAX_PACKET_SIZE],
    )


def _check_reliability_timeouts(config: ConfigType, radio: ConfigType) -> None:
    """A reliability timeout shorter than a packet and its acknowledgement take would send packets again too soon."""
    for path, layer in reliability_layers(config):
        if (timeout := layer.get(CONF_TIMEOUT)) is None:
            continue
        needed = reliability_timeout_ms(
            radio,
            config[CONF_MAX_PACKET_SIZE],
            layer[CONF_ACK_DELAY].total_milliseconds,
        )
        if timeout.total_milliseconds < needed:
            raise cv.Invalid(
                f"With these radio settings a packet and its acknowledgement can take {needed} ms, so this must be "
                f"at least {needed}ms; leave it out to use that",
                path=[*path, CONF_TIMEOUT],
            )


def radio_link_schema(
    class_: MockObjClass, radio_id_key: str, radio_class: MockObjClass
) -> cv.Schema:
    """Schema for a radio link, referring to its radio by radio_id_key."""
    return packet_interface_schema(
        class_, MAX_PACKET_SIZE, MAX_PACKET_SIZE, chooses_reliability_timeout=True
    ).extend({cv.GenerateID(radio_id_key): cv.use_id(radio_class)})


def _two_way_layer(config: ConfigType) -> str | None:
    """The first layer on the link or in a peer that keeps state for one peer, so needs packets back from it."""
    for stack in (config, *config.get(CONF_ADDRESSING, {}).get(CONF_PEERS, [])):
        for kind in STACK_ORDER:
            if (layer := stack.get(kind.key)) is not None and kind.get_type(
                layer
            ).single_peer(layer):
                return kind.key
    return None


def radio_link_final_validate(
    radio_id_key: str,
    payload_length_key: str,
    rx_start_key: str,
    check: Callable[[ConfigType, ConfigType], None] | None = None,
) -> Callable[[ConfigType], ConfigType]:
    """Final validation for a radio link: packets vary in length, the radio listens after sending when a layer needs
    replies, and check(link, radio) checks the rest.
    """

    def validator(config: ConfigType) -> ConfigType:
        radio = fv.get_declaring_config(config[radio_id_key])
        if radio[payload_length_key] != 0:
            raise cv.Invalid(
                f"Packets vary in length, so the radio's '{payload_length_key}' must be 0",
                path=[radio_id_key],
            )
        if not radio[rx_start_key] and (layer := _two_way_layer(config)) is not None:
            raise cv.Invalid(
                f"This '{layer}' needs packets from the peer, but the radio sleeps after sending unless "
                f"'{rx_start_key}' is true",
                path=[radio_id_key],
            )
        if check is not None:
            check(config, radio)
        _check_transmit_time(config, radio)
        _check_reliability_timeouts(config, radio)
        return config

    return validator


async def new_radio_link(
    config: ConfigType,
    radio_id_key: str,
    receive_limit: Callable[[ConfigType], int] = lambda radio: MAX_PACKET_SIZE,
) -> None:
    """Create a radio link, with timings worked out from the radio's settings.

    receive_limit gives the largest packet the radio receives, from any device on the channel.
    """
    radio_config = fv.get_declaring_config(config[radio_id_key], CORE.config)
    # A reliability layer without a timeout gets one long enough for a packet and its acknowledgement
    for _, layer in reliability_layers(config):
        if CONF_TIMEOUT not in layer:
            needed = reliability_timeout_ms(
                radio_config,
                config[CONF_MAX_PACKET_SIZE],
                layer[CONF_ACK_DELAY].total_milliseconds,
            )
            layer[CONF_TIMEOUT] = cv.TimePeriodMilliseconds(
                milliseconds=max(DEFAULT_TIMEOUT.total_milliseconds, needed)
            )
    radio = await cg.get_variable(config[radio_id_key])
    var = await new_packet_interface(config, radio, config[CONF_MAX_PACKET_SIZE])
    cg.add(var.set_max_busy(max_busy_ms(radio_config, receive_limit(radio_config))))
