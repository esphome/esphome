from collections.abc import Callable, Iterable
from dataclasses import dataclass, field
import logging
from typing import Any, Literal

from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_RMT_SYMBOLS, CONF_USE_DMA
from esphome.core import CORE, ID
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
RMT_MEMORY: dict[str, RMTMemory] = {
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

# ESP-IDF refuses more DMA symbols on a TX channel ("mem_block_symbols can't exceed 2046"
# or 2047, depending on the version; 2047 is odd and refused anyway)
MAX_TX_DMA_SYMBOLS: int = 2046

# ESP-IDF's log line for a channel that cannot be allocated
_IDF_ERRORS: dict[str, str] = {
    "TX": "'no free tx channels'",
    "RX": "'no free rx channels'",
    "TX/RX": "'no free tx channels' or 'no free rx channels'",
}


@dataclass
class _Channel:
    id: ID
    name: str | None
    direction: Direction
    symbols: int
    dma: bool


def _label(channel: _Channel) -> str:
    """The name, else the id; an id ESPHome generated is replaced by the config position."""
    if channel.name:
        return channel.name
    if not channel.id.is_manual:
        path = fv.full_config.get().get_path_for_id(channel.id)
        if len(path) > 2 and isinstance(path[1], int):
            return f"{path[0]}[{path[1]}]"
    return str(channel.id)


@dataclass
class _Error:
    channels: list[_Channel]
    message: str
    # Config key the error is shown on, None for the component itself
    key: str | None = None


@dataclass
class ESP32RMTData:
    channels: list[_Channel] = field(default_factory=list)
    checked: bool = False
    errors: list[_Error] = field(default_factory=list)


def _get_data() -> ESP32RMTData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = ESP32RMTData()
    return CORE.data[DOMAIN]


def _get_memory() -> RMTMemory | None:
    if not CORE.is_esp32:
        return None
    return RMT_MEMORY.get(esp32.get_esp32_variant())


def _max_blocks(direction: Direction, memory: RMTMemory) -> int:
    """Blocks one channel can use: a TX channel can extend into the RX blocks behind it."""
    if memory.rx_blocks is None:
        return memory.tx_blocks
    if direction == "TX":
        return memory.tx_blocks + memory.rx_blocks
    return memory.rx_blocks


def request_rmt_channel(direction: Direction) -> Callable[[ConfigType], ConfigType]:
    """Return a CONFIG_SCHEMA validator for a component that creates one RMT channel.

    It checks the entry's rmt_symbols against what ESP-IDF accepts, and records the
    channel so that final_validate_rmt_channels() can check all RMT components together.
    Add it after the schema, so rmt_symbols has its default, and set
    FINAL_VALIDATE_SCHEMA = esp32_rmt.final_validate_rmt_channels in the same component.
    The entry needs an id and rmt_symbols on every variant with RMT; use_dma is optional.
    """

    def _validator(config: ConfigType) -> ConfigType:
        if (memory := _get_memory()) is None:
            return config
        variant = esp32.get_esp32_variant()
        symbols = config[CONF_RMT_SYMBOLS]
        dma = bool(config.get(CONF_USE_DMA))
        size = memory.symbols_per_block
        if symbols < size or symbols % 2:
            raise cv.Invalid(
                f"Must be an even number of at least {size} on {variant}, "
                "the size of one RMT memory block",
                path=[CONF_RMT_SYMBOLS],
            )
        if dma and direction == "TX" and symbols > MAX_TX_DMA_SYMBOLS:
            raise cv.Invalid(
                f"Must be at most {MAX_TX_DMA_SYMBOLS} with '{CONF_USE_DMA}'",
                path=[CONF_RMT_SYMBOLS],
            )
        if not dma and symbols > (limit := _max_blocks(direction, memory) * size):
            raise cv.Invalid(
                f"Must be at most {limit} on {variant} without '{CONF_USE_DMA}': one "
                f"RMT {direction} channel cannot use more memory",
                path=[CONF_RMT_SYMBOLS],
            )
        _get_data().channels.append(
            _Channel(config[CONF_ID], config.get(CONF_NAME), direction, symbols, dma)
        )
        return config

    return _validator


def _labels(channels: list[_Channel]) -> str:
    return ", ".join(f"'{_label(channel)}'" for channel in channels)


def _check_pool(
    direction: str,
    channels: list[_Channel],
    blocks: int,
    memory: RMTMemory,
    idf_errors: str,
) -> _Error | None:
    """Return the error if the channels cannot all be allocated, warn if they may not fit.

    direction is "TX", "RX", or "TX/RX" for the shared pool of the ESP32 and ESP32-S2.
    """
    variant = esp32.get_esp32_variant()
    if len(channels) > blocks:
        kind = "shared by TX and RX" if direction == "TX/RX" else f"for {direction}"
        return _Error(
            channels,
            f"{variant} has {blocks} RMT channels {kind}, but these {len(channels)} "
            f"components each need one: {_labels(channels)}. Remove some of them or "
            "use a chip with more RMT channels",
        )
    dma = [channel for channel in channels if channel.dma]
    if len(dma) > 1:
        return _Error(
            dma,
            f"Only one RMT {direction} channel on {variant} supports DMA, but "
            f"'{CONF_USE_DMA}' is set on {_labels(dma)}. Keep it on one of them",
            CONF_USE_DMA,
        )
    # The driver rounds rmt_symbols up to whole blocks; a DMA channel always uses one block
    size = memory.symbols_per_block
    others = [channel for channel in channels if not channel.dma]
    used = len(dma) + sum(-(-channel.symbols // size) for channel in others)
    if used <= blocks:
        return None
    fair = (blocks - len(dma)) // len(others) * size
    usage = ", ".join(
        f"'{_label(channel)}' ({'DMA' if channel.dma else channel.symbols})"
        for channel in channels
    )
    _LOGGER.warning(
        "RMT %s memory on %s holds %d blocks of %d symbols, but these components "
        "need %d blocks: %s. Set '%s: %d' on %s. Otherwise a component may fail at "
        "boot: ESP-IDF logs %s, and ESPHome logs 'Channel creation failed' or 'out "
        "of RMT symbol memory'.",
        direction,
        variant,
        blocks,
        size,
        used,
        usage,
        CONF_RMT_SYMBOLS,
        fair,
        _labels([channel for channel in others if channel.symbols > fair]),
        idf_errors,
    )
    return None


def _check_budget(memory: RMTMemory, channels: list[_Channel]) -> list[_Error]:
    if memory.rx_blocks is None:
        pools = [("TX/RX", channels, memory.tx_blocks, _IDF_ERRORS["TX/RX"])]
    else:
        tx = [channel for channel in channels if channel.direction == "TX"]
        rx = [channel for channel in channels if channel.direction == "RX"]
        # A TX channel can extend into the RX blocks. A single one without receivers
        # always fits, request_rmt_channel() already checked its size. With receivers,
        # whether it fits depends on the order the channels are created in, and the
        # receiver can be the one that fails.
        tx_blocks = memory.tx_blocks
        if len(tx) == 1 and not rx:
            tx_blocks += memory.rx_blocks
        tx_errors = _IDF_ERRORS["TX/RX" if rx else "TX"]
        pools = [
            ("TX", tx, tx_blocks, tx_errors),
            ("RX", rx, memory.rx_blocks, _IDF_ERRORS["RX"]),
        ]
    errors = [_check_pool(*pool[:3], memory, pool[3]) for pool in pools]
    return [error for error in errors if error is not None]


def final_validate_rmt_channels(config: ConfigType) -> None:
    """FINAL_VALIDATE_SCHEMA of the components that use request_rmt_channel().

    Fails when the RMT components cannot all get a channel, warns when they request more
    memory than the chip has. The check runs once; its errors are raised from the first
    component that causes one, since final validation stops after the first error.
    """
    data = _get_data()
    if not data.checked:
        data.checked = True
        # Skip in testing mode, where component grouping merges the RMT components of many tests
        if not CORE.testing_mode and (memory := _get_memory()) is not None:
            data.errors = _check_budget(memory, data.channels)
    own_id = config.get(CONF_ID)
    for error in data.errors:
        if any(channel.id is own_id for channel in error.channels):
            # Errors of other components are not shown on this component's key
            key = error.key if len(data.errors) == 1 else None
            raise cv.Invalid(
                "\n".join(error.message for error in data.errors),
                path=[key] if key else [],
            )


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
