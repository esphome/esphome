"""How long a radio packet takes to send, for the listen before talk limit of the radio links."""

import math

from esphome.types import ConfigType

# Listen before talk holds a send back for at most the airtime of the largest packet, and never less than this
MIN_MAX_BUSY_MS = 2000
# Room for the preamble detection and the radio's own delays
MARGIN = 1.25


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
    """How long listen before talk may hold a send back."""
    return max(
        MIN_MAX_BUSY_MS, math.ceil(packet_airtime_ms(radio, max_packet_size) * MARGIN)
    )
