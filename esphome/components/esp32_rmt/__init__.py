from collections.abc import Callable, Iterable
from dataclasses import dataclass, field, replace
from itertools import permutations
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
    # False for a channel of a fixed size, from request_rmt_channels()
    adjustable: bool = True
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


def request_rmt_channels(
    channels: Iterable[tuple[Direction, int]], setup_priority: float
) -> Callable[[ConfigType], ConfigType]:
    """Return a CONFIG_SCHEMA validator for a component with RMT channels of a fixed size.

    Like request_rmt_channel(), but for a component that has no rmt_symbols option.
    channels lists the direction and the memory blocks of each channel, in the order
    setup() creates them. All of them are recorded under the id of the entry. The check
    expects setup() to stop at the first channel it cannot create. Use
    final_validate_rmt_channels() and register_rmt_channel() as with
    request_rmt_channel().
    """
    channels = tuple(channels)

    def _validator(config: ConfigType) -> ConfigType:
        if (memory := _get_memory()) is None:
            return config
        # Wrong values here are a bug in the component, so all of them are checked
        # before any channel is recorded
        for direction, blocks in channels:
            if direction not in _IDF_ERRORS:
                raise cv.Invalid(
                    f"'{config[CONF_ID]}' requests an RMT channel in the direction "
                    f"'{direction}', but it must be 'TX' or 'RX'"
                )
            limit = _max_blocks(direction, memory)
            if not 1 <= blocks <= limit:
                raise cv.Invalid(
                    f"'{config[CONF_ID]}' requests an RMT {direction} channel of "
                    f"{blocks} blocks, but on {esp32.get_esp32_variant()} one "
                    f"{direction} channel must have 1 to {limit} blocks"
                )
        _get_data().channels.extend(
            _Channel(
                config[CONF_ID],
                config.get(CONF_NAME),
                direction,
                blocks * memory.symbols_per_block,
                False,
                config.get(CONF_SETUP_PRIORITY, setup_priority),
                adjustable=False,
            )
            for direction, blocks in channels
        )
        return config

    return _validator


def _by_component(channels: list[_Channel]) -> dict[ID, list[_Channel]]:
    """The channels of each component, by its id, in the order of the channels."""
    components: dict[ID, list[_Channel]] = {}
    for channel in channels:
        components.setdefault(channel.id, []).append(channel)
    return components


def _labels(channels: list[_Channel]) -> str:
    """The label of each component once, with the count of one with several channels.

    Components are told apart by id: two of them can have the same label, when the name
    of one is the id of the other.
    """
    return ", ".join(
        f"'{own[0].label}'" + (f" ({len(own)} channels)" if len(own) > 1 else "")
        for own in _by_component(channels).values()
    )


def _check_pool(direction: str, channels: list[_Channel], blocks: int) -> _Error | None:
    """Return the error if the channels cannot all get a channel.

    direction is "TX", "RX", or "TX/RX" for the shared pool of the ESP32 and ESP32-S2.
    """
    variant = esp32.get_esp32_variant()
    if len(channels) > blocks:
        kind = "shared by TX and RX" if direction == "TX/RX" else f"for {direction}"
        if len({channel.id for channel in channels}) == len(channels):
            need = f"these {len(channels)} components each need one"
        else:
            need = f"these components need {len(channels)}"
        return _Error(
            channels,
            f"{variant} has {blocks} RMT channels {kind}, but {need}: "
            f"{_labels(channels)}. Remove some of them or use a chip with more RMT "
            "channels",
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
    The setup() of a component stops at its first channel that fails, so its later
    channels are not created and not returned.
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
    stopped: set[ID] = set()
    for channel in channels:
        if channel.id in stopped:
            continue
        mask = (1 << channel.blocks(size)) - 1
        candidates = starts[channel.direction]
        if channel.dma:
            candidates = candidates[-1:]
        start = next((j for j in candidates if not occupied & mask << j), None)
        if start is None:
            failed.append(channel)
            stopped.add(channel.id)
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


def _reorder(
    memory: RMTMemory, components: list[list[_Channel]], named: bool = False
) -> str | None:
    """Suggest setup_priority when the components fit in another setup order.

    setup_priority moves a whole component: its setup() creates its channels in order.
    The order with the smaller components first is tried first, then every other one.
    When named is set, the order is always given by the names of the components.
    """
    size = memory.symbols_per_block
    channels = [channel for own in components for channel in own]
    # No order fits more blocks than the memory has, so do not try them all
    if sum(c.blocks(size) for c in channels) > memory.total_blocks or (
        memory.rx_blocks is not None
        and sum(c.blocks(size) for c in channels if c.direction == "RX")
        > memory.rx_blocks
    ):
        return None
    smallest_first = sorted(
        components, key=lambda own: sum(c.blocks(size) for c in own)
    )
    # Orders that only swap components with the same channels fit the same way
    seen: set[tuple[tuple[tuple[str, int, bool], ...], ...]] = set()
    # The first order is smallest_first itself
    for i, order in enumerate(permutations(smallest_first)):
        shape = tuple(
            tuple((c.direction, c.blocks(size), c.dma) for c in own) for own in order
        )
        if shape in seen:
            continue
        seen.add(shape)
        if _allocate(memory, [channel for own in order for channel in own]):
            continue
        if i == 0 and not named:
            return f"set '{CONF_SETUP_PRIORITY}' so the smaller ones are set up first"
        first, *rest = (f"'{own[0].label}'" for own in order)
        then = "".join(f", then {label}" for label in rest)
        return f"set '{CONF_SETUP_PRIORITY}' so {first} is set up first{then}"
    return None


def _advice(memory: RMTMemory, channels: list[_Channel], reorder: str | None) -> str:
    """Suggest an rmt_symbols value for the components of each overrun pool.

    A DMA channel and a channel of a fixed size keep their blocks, whatever rmt_symbols
    is. When the TX channels need more than the TX blocks even with one block each, the
    rest can come from the RX blocks, because a TX channel can extend into them. The
    values are only suggested when the channels then fit in this order, or in another
    order that is then suggested with them. When a pool has too little memory, or there
    is nothing to reduce, suggest to remove components instead. reorder is the advice
    from _reorder(), suggested instead when it is set.
    """
    size = memory.symbols_per_block
    suggestions = []
    reduced: dict[_Channel, int] = {}
    # The channels of the pools that reducing rmt_symbols cannot fix
    stuck: set[_Channel] = set()
    # RX blocks that TX channels need
    spill = 0
    for direction, own, blocks in _pools(memory, channels):
        blocks -= spill
        if sum(channel.blocks(size) for channel in own) <= blocks:
            continue
        others = [c for c in own if c.adjustable and not c.dma]
        fixed = sum(c.blocks(size) for c in own if c.dma or not c.adjustable)
        fair = (blocks - fixed) // len(others) * size if others else 0
        # Give each TX channel that can be reduced one block, and take what is
        # missing from the RX blocks
        if (
            fair < size
            and direction == "TX"
            and memory.rx_blocks is not None
            and (need := fixed + len(others) - blocks) <= memory.rx_blocks
        ):
            spill, fair = need, size
        if fair < size:
            stuck.update(own)
            continue
        if shrink := [channel for channel in others if channel.symbols > fair]:
            suggestions.append(f"'{CONF_RMT_SYMBOLS}: {fair}' on {_labels(shrink)}")
            reduced.update((channel, fair) for channel in shrink)
    # Advice on setup_priority that the reduced values need
    together = None
    if suggestions:
        # Check the suggestion on the pools it can fix
        trial = [
            replace(channel, symbols=reduced.get(channel, channel.symbols))
            for channel in channels
            if channel not in stuck
        ]
        if _allocate(memory, trial):
            if not stuck and not reorder:
                together = _reorder(
                    memory, list(_by_component(trial).values()), named=True
                )
            if together is None:
                suggestions = []
    advice = [f"Set {' and '.join(suggestions)}"] if suggestions else []
    if reorder:
        advice.append(reorder)
    elif together:
        advice.append(together)
    elif stuck or not suggestions:
        advice.append(
            "remove some of these components or use a chip with more RMT memory"
        )
    text = (", or " if reorder else " and ").join(advice)
    return text[0].upper() + text[1:]


def final_validate_rmt_channels(config: ConfigType) -> None:
    """FINAL_VALIDATE_SCHEMA of the components that request RMT channels.

    Use it with request_rmt_channel() or request_rmt_channels(). Fails when the RMT
    components cannot all get a channel. The check runs once; its errors are raised
    together from the first component named in one of them, since final validation
    stops after the first error. Whether the channels fit in the RMT memory depends on
    the setup order, which is only known in code generation.
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
    """Record the setup order of a component that requests RMT channels.

    Use it with request_rmt_channel() or request_rmt_channels(). Call it in to_code
    right after cg.register_component(): C++ sets up components of the same priority in
    the order they are registered.
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
    size = memory.symbols_per_block
    components = _by_component(channels)
    # A component with several channels gets the direction of each
    usage = ", ".join(
        f"'{channel.label}' ("
        + (f"{channel.direction} " if len(components[channel.id]) > 1 else "")
        + f"{'DMA' if channel.dma else channel.symbols})"
        for channel in channels
    )
    _LOGGER.warning(
        "RMT memory on %s holds %d blocks of %d symbols, but in the order ESPHome sets "
        "up these components, %s will not fit: %s. %s. Otherwise ESP-IDF logs %s "
        "at boot, and ESPHome logs 'Channel creation failed' or 'out of RMT symbol "
        "memory'.",
        esp32.get_esp32_variant(),
        memory.total_blocks,
        size,
        _labels(failed),
        usage,
        _advice(memory, channels, _reorder(memory, list(components.values()))),
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
