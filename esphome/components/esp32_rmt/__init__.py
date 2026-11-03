from collections.abc import Callable, Iterable
from dataclasses import dataclass, field
import logging
from typing import Any, Literal

from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_PLATFORM,
    CONF_RMT_SYMBOLS,
    CONF_USE_DMA,
)
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CODEOWNERS = ["@jesserockz"]
DOMAIN = "esp32_rmt"

VARIANTS_NO_RMT = {esp32.VARIANT_ESP32C2, esp32.VARIANT_ESP32C61}


@dataclass(frozen=True)
class RMTMemory:
    """RMT channel memory of a variant.

    From SOC_RMT_MEM_WORDS_PER_CHANNEL in ESP-IDF soc_caps.h and the TX/RX channel counts
    (SOC_RMT_*_CANDIDATES_PER_GROUP in soc_caps.h up to 5.5, RMT_LL_* in hal/rmt_ll.h from 6.0).
    """

    symbols_per_block: int
    tx_blocks: int
    # None when TX and RX channels draw from one pool of tx_blocks (ESP32, ESP32-S2)
    rx_blocks: int | None


# A variant that is not listed is not checked
RMT_MEMORY = {
    esp32.VARIANT_ESP32: RMTMemory(64, 8, None),
    esp32.VARIANT_ESP32S2: RMTMemory(64, 4, None),
    esp32.VARIANT_ESP32S3: RMTMemory(48, 4, 4),
    esp32.VARIANT_ESP32S31: RMTMemory(48, 4, 4),
    esp32.VARIANT_ESP32P4: RMTMemory(48, 4, 4),
    esp32.VARIANT_ESP32C3: RMTMemory(48, 2, 2),
    esp32.VARIANT_ESP32C5: RMTMemory(48, 2, 2),
    esp32.VARIANT_ESP32C6: RMTMemory(48, 2, 2),
    esp32.VARIANT_ESP32H2: RMTMemory(48, 2, 2),
    esp32.VARIANT_ESP32H4: RMTMemory(48, 2, 2),
    esp32.VARIANT_ESP32H21: RMTMemory(48, 2, 2),
}

Direction = Literal["TX", "RX"]


@dataclass(frozen=True, order=True)
class _Consumer:
    domain: str
    # Empty for a component that is not a platform
    platform: str
    direction: Direction


# Filled when the RMT components are imported, which happens before final validation
_CONSUMERS: set[_Consumer] = set()


# ESP-IDF's log line for a channel that cannot be allocated
_IDF_ERRORS = {
    "TX": "'no free tx channels'",
    "RX": "'no free rx channels'",
    "TX/RX": "'no free tx channels' or 'no free rx channels'",
}


@dataclass(frozen=True)
class _Channel:
    label: str
    symbols: int
    dma: bool
    entry: ConfigType = field(compare=False, repr=False)


@dataclass
class _Error:
    channels: list[_Channel]
    message: str


@dataclass
class ESP32RMTData:
    budget_checked: bool = False
    errors: list[_Error] = field(default_factory=list)


def _get_data() -> ESP32RMTData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = ESP32RMTData()
    return CORE.data[DOMAIN]


def _channels(domain: str, platform: str) -> list[_Channel]:
    channels = []
    for index, entry in enumerate(fv.full_config.get().get(domain, [])):
        if platform and entry.get(CONF_PLATFORM) != platform:
            continue
        if name := entry.get(CONF_NAME):
            label = name
        elif entry[CONF_ID].is_manual:
            label = str(entry[CONF_ID])
        else:
            label = f"{domain}[{index}]"
        channels.append(
            _Channel(
                label, entry[CONF_RMT_SYMBOLS], bool(entry.get(CONF_USE_DMA)), entry
            )
        )
    return channels


def _labels(channels: list[_Channel]) -> str:
    return ", ".join(f"'{channel.label}'" for channel in channels)


def _check_pool(
    direction: str, channels: list[_Channel], blocks: int, memory: RMTMemory
) -> _Error | None:
    """Return the error if the channels cannot all be allocated, warn if they may not fit."""
    variant = esp32.get_esp32_variant()
    if len(channels) > blocks:
        return _Error(
            channels,
            f"{variant} has {blocks} RMT {direction} channels, but these "
            f"{len(channels)} components each need one: {_labels(channels)}",
        )
    dma = [channel for channel in channels if channel.dma]
    if len(dma) > 1:
        return _Error(
            dma,
            f"Only one RMT {direction} channel on {variant} supports DMA, but "
            f"'{CONF_USE_DMA}' is set on {_labels(dma)}",
        )
    # The driver rounds rmt_symbols up to whole blocks; a DMA channel always uses one block
    size = memory.symbols_per_block
    others = [channel for channel in channels if not channel.dma]
    used = len(dma) + sum(-(-channel.symbols // size) for channel in others)
    if used <= blocks:
        return None
    fair = (blocks - len(dma)) // len(others) * size
    usage = ", ".join(
        f"'{channel.label}' ({'DMA' if channel.dma else channel.symbols})"
        for channel in channels
    )
    _LOGGER.warning(
        "RMT %s memory on %s holds %d blocks of %d symbols, but these components "
        "need %d blocks: %s. The ones that do not fit can fail at boot with %s "
        "('Channel creation failed' or 'out of RMT symbol memory' in the ESPHome log). "
        "Set '%s: %d' on %s.",
        direction,
        variant,
        blocks,
        size,
        used,
        usage,
        _IDF_ERRORS[direction],
        CONF_RMT_SYMBOLS,
        fair,
        _labels([channel for channel in others if channel.symbols > fair]),
    )
    return None


def _check_budget() -> list[_Error]:
    variant = esp32.get_esp32_variant()
    if (memory := RMT_MEMORY.get(variant)) is None:
        return []
    channels: dict[Direction, list[_Channel]] = {"TX": [], "RX": []}
    for consumer in sorted(_CONSUMERS):
        channels[consumer.direction] += _channels(consumer.domain, consumer.platform)
    tx, rx = channels["TX"], channels["RX"]
    # The driver rejects these when it creates the channel
    size = memory.symbols_per_block
    errors: list[_Error | None] = [
        _Error(
            [channel],
            f"'{CONF_RMT_SYMBOLS}' must be even and at least {size} on {variant}, "
            f"but '{channel.label}' sets {channel.symbols}",
        )
        for channel in tx + rx
        if channel.symbols < size or channel.symbols % 2
    ]
    if memory.rx_blocks is None:
        pools = [("TX/RX", tx + rx, memory.tx_blocks)]
    else:
        pools = [("TX", tx, memory.tx_blocks), ("RX", rx, memory.rx_blocks)]
    errors += [_check_pool(*pool, memory) for pool in pools]
    return [error for error in errors if error is not None]


def _validate_channels(config: ConfigType) -> None:
    data = _get_data()
    if not data.budget_checked:
        data.budget_checked = True
        # Skip in testing mode, where component grouping merges the RMT components of many tests
        if not CORE.testing_mode and CORE.is_esp32:
            data.errors = _check_budget()
    if any(
        channel.entry is config for error in data.errors for channel in error.channels
    ):
        raise cv.Invalid("\n".join(error.message for error in data.errors))


def final_validate_rmt_channel(
    direction: Direction, domain: str, platform: str = ""
) -> Callable[[ConfigType], None]:
    """Return the FINAL_VALIDATE_SCHEMA of a component that creates one RMT channel per entry.

    domain and platform say where its entries are in the config, for example "light" and
    "esp32_rmt_led_strip" for a light platform, or only "remote_receiver". Each entry's
    rmt_symbols and use_dma are counted against the chip's RMT channels and memory together
    with all other RMT components. The check runs once; its errors are raised from the first
    component that causes one, since final validation stops after the first error.
    """
    _CONSUMERS.add(_Consumer(domain, platform, direction))
    return _validate_channels


def validate_rmt_not_supported(
    rmt_only_keys: Iterable[str],
) -> Callable[[ConfigType], ConfigType]:
    """Validate that RMT-only config keys are not used on variants without RMT hardware."""
    rmt_only_keys = set(rmt_only_keys)

    def _validator(config: ConfigType) -> ConfigType:
        if CORE.is_esp32:
            variant = esp32.get_esp32_variant()
            if variant in VARIANTS_NO_RMT:
                unsupported = rmt_only_keys.intersection(config)
                if unsupported:
                    keys = ", ".join(sorted(f"'{k}'" for k in unsupported))
                    raise cv.Invalid(
                        f"{keys} not available on {variant} (no RMT hardware)"
                    )
        return config

    return _validator


def validate_clock_resolution() -> Callable[[Any], int]:
    def _validator(value: Any) -> int:
        cv.only_on_esp32(value)
        value = cv.int_(value)
        variant = esp32.get_esp32_variant()
        if variant == esp32.VARIANT_ESP32H2 and value > 32000000:
            raise cv.Invalid(
                f"ESP32 variant {variant} has a max clock_resolution of 32000000."
            )
        if value > 80000000:
            raise cv.Invalid(
                f"ESP32 variant {variant} has a max clock_resolution of 80000000."
            )
        return value

    return _validator
