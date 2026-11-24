from collections.abc import Callable, Iterable
from dataclasses import dataclass, field
import logging
from typing import Any, Literal

from esphome.components import esp32
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_RMT_SYMBOLS,
    CONF_SETUP_PRIORITY,
    CONF_USE_DMA,
)
from esphome.core import CORE, ID
from esphome.coroutine import CoroPriority, coroutine_with_priority
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

    @property
    def total_blocks(self) -> int:
        return self.tx_blocks + (self.rx_blocks or 0)


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
}


@dataclass(eq=False)
class _Channel:
    id: ID
    name: str | None
    direction: Direction
    symbols: int
    dma: bool
    setup_priority: float
    # Set in final validation, where the config path of a generated id is known
    label: str = ""

    def blocks(self, size: int) -> int:
        """Blocks the driver allocates: rmt_symbols rounded up, one for a DMA channel."""
        return 1 if self.dma else -(-self.symbols // size)


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
    # IDs of the RMT components in the order code generation registers them
    registered: list[ID] = field(default_factory=list)


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


def request_rmt_channel(
    direction: Direction, setup_priority: float
) -> Callable[[ConfigType], ConfigType]:
    """Return a CONFIG_SCHEMA validator for a component that creates one RMT channel.

    It checks the entry's rmt_symbols against what ESP-IDF accepts, and records the
    channel so that final_validate_rmt_channels() can check all RMT components together.
    Add it after the schema, so rmt_symbols has its default, and set
    FINAL_VALIDATE_SCHEMA = esp32_rmt.final_validate_rmt_channels in the same component.
    The entry needs an id and rmt_symbols on every variant with RMT; use_dma is optional.
    setup_priority is the component's get_setup_priority() in C++, which creates the
    channel in setup(); the entry's setup_priority overrides it. Call
    register_rmt_channel() in to_code.
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
            _Channel(
                config[CONF_ID],
                config.get(CONF_NAME),
                direction,
                symbols,
                dma,
                config.get(CONF_SETUP_PRIORITY, setup_priority),
            )
        )
        return config

    return _validator


def _labels(channels: list[_Channel]) -> str:
    return ", ".join(f"'{channel.label}'" for channel in channels)


def _check_pool(direction: str, channels: list[_Channel], blocks: int) -> _Error | None:
    """Return the error if the channels cannot all get a channel.

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
    return None


def _allocate(memory: RMTMemory, channels: list[_Channel]) -> list[_Channel]:
    """Allocate the channels in this order like ESP-IDF and return the ones that fail.

    The driver gives a channel the first free run of blocks that starts at one of the
    channels of its direction, and a DMA channel the last channel of its direction.
    """
    size = memory.symbols_per_block
    total = memory.total_blocks
    starts = {
        "TX": range(memory.tx_blocks),
        # On the ESP32 and ESP32-S2 the RX channels are the same as the TX channels
        "RX": range(total - (memory.rx_blocks or memory.tx_blocks), total),
    }
    # Blocks past the end of the RMT memory count as taken, like in ESP-IDF
    occupied = ~((1 << total) - 1)
    failed = []
    for channel in channels:
        mask = (1 << channel.blocks(size)) - 1
        candidates = starts[channel.direction]
        if channel.dma:
            candidates = candidates[-1:]
        start = next((j for j in candidates if not occupied & mask << j), None)
        if start is None:
            failed.append(channel)
        else:
            occupied |= mask << start
    return failed


def _pools(
    memory: RMTMemory, channels: list[_Channel]
) -> list[tuple[str, list[_Channel], int]]:
    """The channel pools of the variant: direction, its channels and its blocks."""
    if memory.rx_blocks is None:
        return [("TX/RX", channels, memory.tx_blocks)]
    return [
        (direction, [c for c in channels if c.direction == direction], blocks)
        for direction, blocks in (("TX", memory.tx_blocks), ("RX", memory.rx_blocks))
    ]


def _suggestions(
    memory: RMTMemory, pools: list[tuple[str, list[_Channel], int]]
) -> list[str]:
    """Suggest an rmt_symbols value for the components of each overrun pool."""
    size = memory.symbols_per_block
    suggestions = []
    for _, channels, blocks in pools:
        if sum(channel.blocks(size) for channel in channels) <= blocks:
            continue
        others = [channel for channel in channels if not channel.dma]
        fair = (blocks - (len(channels) - len(others))) // len(others) * size
        shrink = [channel for channel in others if channel.symbols > fair]
        suggestions.append(f"'{CONF_RMT_SYMBOLS}: {fair}' on {_labels(shrink)}")
    return suggestions


def final_validate_rmt_channels(config: ConfigType) -> None:
    """FINAL_VALIDATE_SCHEMA of the components that use request_rmt_channel().

    Fails when the RMT components cannot all get a channel. The check runs once; its
    errors are raised together from the first component named in one of them, since
    final validation stops after the first error. Whether the channels fit in the RMT
    memory depends on the setup order, which is only known in code generation.
    """
    data = _get_data()
    if not data.checked:
        data.checked = True
        for channel in data.channels:
            channel.label = _label(channel)
        # Skip in testing mode, where component grouping merges the RMT components of many tests
        if not CORE.testing_mode and (memory := _get_memory()) is not None:
            data.errors = [
                error
                for pool in _pools(memory, data.channels)
                if (error := _check_pool(*pool)) is not None
            ]
    own_id = config.get(CONF_ID)
    if not any(c.id == own_id for error in data.errors for c in error.channels):
        return
    # Errors of other components are not shown on this component's key
    key = data.errors[0].key if len(data.errors) == 1 else None
    raise cv.Invalid(
        "\n".join(error.message for error in data.errors),
        path=[key] if key else [],
    )


def register_rmt_channel(config: ConfigType) -> None:
    """Record the setup order of a component that uses request_rmt_channel().

    Call it in to_code right after cg.register_component(): C++ sets up components of
    the same priority in the order they are registered.
    """
    data = _get_data()
    if not data.registered:
        CORE.add_job(_check_memory)
    data.registered.append(config[CONF_ID])


@coroutine_with_priority(CoroPriority.FINAL)
async def _check_memory() -> None:
    """Warn when the RMT channels, allocated in setup order, do not fit in the RMT memory."""
    data = _get_data()
    if CORE.testing_mode or (memory := _get_memory()) is None:
        return
    # Application::setup() sorts the components by priority with a stable sort
    rank = {channel_id: i for i, channel_id in enumerate(data.registered)}
    channels = sorted(data.channels, key=lambda c: rank.get(c.id, len(rank)))
    channels.sort(key=lambda channel: -channel.setup_priority)
    if not (failed := _allocate(memory, channels)):
        return
    fix = ""
    size = memory.symbols_per_block
    if not _allocate(memory, sorted(channels, key=lambda c: c.blocks(size))):
        fix = f", or set '{CONF_SETUP_PRIORITY}' so the smaller ones are set up first"
    usage = ", ".join(
        f"'{channel.label}' ({'DMA' if channel.dma else channel.symbols})"
        for channel in channels
    )
    _LOGGER.warning(
        "RMT memory on %s holds %d blocks of %d symbols, but in the order ESPHome sets "
        "up these components, %s will not fit: %s. Set %s%s. Otherwise ESP-IDF logs %s "
        "at boot, and ESPHome logs 'Channel creation failed' or 'out of RMT symbol "
        "memory'.",
        esp32.get_esp32_variant(),
        memory.total_blocks,
        size,
        _labels(failed),
        usage,
        " and ".join(_suggestions(memory, _pools(memory, channels))),
        fix,
        " or ".join(sorted({_IDF_ERRORS[channel.direction] for channel in failed})),
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
