import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components import remote_base
from esphome.components.libretiny import get_libretiny_family
from esphome.components.libretiny.const import FAMILY_BK7238, KEY_BOARD, KEY_LIBRETINY
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_DUMP,
    CONF_FILTER,
    CONF_ID,
    CONF_IDLE,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_PIN,
    CONF_TOLERANCE,
    CONF_TYPE,
    CONF_VALUE,
)
from esphome.core import CORE, TimePeriod
import esphome.final_validate as fv
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["remote_base"]
CODEOWNERS = ["@bbbert"]
DEPENDENCIES = ["bk72xx"]
DOMAIN = "beken_irda"

CONF_SAMPLE_PERIOD = "sample_period"

MODE_HARDWARE = "hardware"
MODE_TIMER = "timer"

# Matches sample_period_ in beken_irda.h.
DEFAULT_SAMPLE_PERIOD_US = 100

TIMER_ONLY_KEYS = (CONF_SAMPLE_PERIOD, CONF_FILTER, CONF_IDLE, CONF_BUFFER_SIZE)

beken_irda_ns = cg.esphome_ns.namespace("beken_irda")

BekenIrdaComponent = beken_irda_ns.class_(
    "BekenIrdaComponent", cg.Component, remote_base.RemoteReceiverBase
)


def validate_pin(value: ConfigType) -> ConfigType:
    """The frontends read the receiver output active-low and do not apply an inverted pin."""
    if value[CONF_INVERTED]:
        raise cv.Invalid(
            "beken_irda does not support an inverted pin; the receiver output is read active-low"
        )
    return value


def validate_hardware_pin(value: ConfigType) -> ConfigType:
    """Hardware mode has no pin matrix, so only the fixed IRDA pin works."""
    if value[CONF_NUMBER] != 26:
        raise cv.Invalid(
            f"Hardware mode only accepts the IRDA pin P26, not pin {value[CONF_NUMBER]}; "
            f"use mode: {MODE_TIMER} for other pins"
        )
    return value


def _has_legacy_irda_block() -> bool:
    """Whether the board has the legacy IRDA block the component models."""
    if get_libretiny_family() == FAMILY_BK7238:
        # The BK7238 has no IRDA block; the registers read 0.
        return False
    # BK7252N/BK7253 have a different block, but currently report the BK7251 family, so the board
    # id is the only way to tell them apart.
    board = CORE.data[KEY_LIBRETINY][KEY_BOARD].lower()
    return "bk7252n" not in board and "bk7253" not in board


def validate_hardware_board(config: ConfigType) -> ConfigType:
    """Reject parts whose IRDA block cannot use the legacy NEC register set."""
    if get_libretiny_family() == FAMILY_BK7238:
        raise cv.Invalid(
            f"Hardware mode is not supported on {FAMILY_BK7238} (no IRDA block); use mode: {MODE_TIMER}"
        )
    if not _has_legacy_irda_block():
        board = CORE.data[KEY_LIBRETINY][KEY_BOARD]
        raise cv.Invalid(
            f"Hardware mode is not supported on '{board}'; use mode: {MODE_TIMER}"
        )
    return config


def validate_config(config: ConfigType) -> ConfigType:
    if config[CONF_MODE] == MODE_HARDWARE:
        validate_hardware_board(config)
        validate_hardware_pin(config[CONF_PIN])
        return config

    if (filter_us := config.get(CONF_FILTER)) is not None:
        # The C++ default sample period applies when the option is not set, so a user who only
        # sets `filter` is still warned when it cannot do anything.
        sample_us = config.get(
            CONF_SAMPLE_PERIOD, TimePeriod(microseconds=DEFAULT_SAMPLE_PERIOD_US)
        )
        if filter_us.total_microseconds < sample_us.total_microseconds:
            _LOGGER.warning(
                "beken_irda: filter is shorter than the sample period and will be clamped to one sample"
            )
    return config


BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BekenIrdaComponent),
        cv.Optional(CONF_PIN, default="P26"): cv.All(
            pins.internal_gpio_input_pin_schema, validate_pin
        ),
        cv.Optional(CONF_MODE, default=MODE_TIMER): cv.one_of(
            MODE_TIMER, MODE_HARDWARE, lower=True
        ),
        cv.Optional(CONF_TOLERANCE, default="30%"): remote_base.validate_tolerance,
        cv.Optional(CONF_DUMP, default=[]): remote_base.validate_dumpers,
        cv.Optional(CONF_SAMPLE_PERIOD): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(
                min=TimePeriod(microseconds=50),
                max=TimePeriod(microseconds=2000),
            ),
        ),
        cv.Optional(CONF_FILTER): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(max=TimePeriod(microseconds=4294967295)),
        ),
        cv.Optional(CONF_IDLE): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(max=TimePeriod(microseconds=4294967295)),
        ),
        cv.Optional(CONF_BUFFER_SIZE): cv.int_range(min=64, max=8192),
    }
).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.All(
    # validate_triggers is lazy, so triggers registered by an external protocol after this module
    # is imported are still accepted.
    remote_base.validate_triggers(BASE_SCHEMA),
    cv.only_when(TIMER_ONLY_KEYS, CONF_MODE, MODE_TIMER),
    validate_config,
)


def final_validate(config: ConfigType) -> None:
    # On BK7238 the remote transmitter's PWM envelope runs on BKTIMER1, the only free 0-2 timer
    # channel, and disables it when the frame ends. Sharing the channel would stop reception after
    # a transmit, so the two cannot be combined in timer mode.
    if (
        config[CONF_MODE] == MODE_TIMER
        and get_libretiny_family() == FAMILY_BK7238
        and "remote_transmitter" in fv.full_config.get()
    ):
        raise cv.Invalid(
            "Timer mode and remote_transmitter both use BKTIMER1 on BK7238; "
            "remove remote_transmitter (hardware mode is unavailable on this chip)"
        )


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin)

    # Timer mode clears the legacy IRDA control register to disable the NEC decoder; the register
    # is only known on the legacy block.
    if _has_legacy_irda_block():
        cg.add_define("USE_BEKEN_IRDA_LEGACY_IRDA")

    cg.add(var.set_hardware(config[CONF_MODE] == MODE_HARDWARE))
    if (sample_period := config.get(CONF_SAMPLE_PERIOD)) is not None:
        cg.add(var.set_sample_period(sample_period.total_microseconds))
    if (filter_us := config.get(CONF_FILTER)) is not None:
        cg.add(var.set_filter_us(filter_us.total_microseconds))
    if (idle := config.get(CONF_IDLE)) is not None:
        cg.add(var.set_idle_us(idle.total_microseconds))
    cg.add_optional(var.set_buffer_size, config.get(CONF_BUFFER_SIZE))
    cg.add(
        var.set_tolerance(
            config[CONF_TOLERANCE][CONF_VALUE], config[CONF_TOLERANCE][CONF_TYPE]
        )
    )

    for dumper in await remote_base.build_dumpers(config[CONF_DUMP]):
        remote_base.add_dumper(var, dumper)

    for trigger in await remote_base.build_triggers(config):
        remote_base.add_listener(var, trigger)

    await cg.register_component(var, config)
