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

beken_irda_ns = cg.esphome_ns.namespace("beken_irda")

BekenIrdaComponent = beken_irda_ns.class_(
    "BekenIrdaComponent", cg.Component, remote_base.RemoteReceiverBase
)


def validate_hardware_pin(value: ConfigType) -> ConfigType:
    """Hardware mode has no pin matrix, so only the fixed IRDA pin works."""
    if value[CONF_NUMBER] != 26:
        raise cv.Invalid(
            f"Hardware mode only accepts the IRDA pin P26, not pin {value[CONF_NUMBER]}; "
            f"use mode: {MODE_TIMER} for other pins"
        )
    return value


def validate_hardware_board(config: ConfigType) -> ConfigType:
    """Reject parts whose IRDA block cannot use the legacy NEC register set."""
    # BK7252N/BK7253 use a different, raw-timing block. Those boards currently report the
    # BK7251 family, so the board id is the only way to tell them apart.
    board = CORE.data[KEY_LIBRETINY][KEY_BOARD].lower()
    if "bk7252n" in board or "bk7253" in board:
        raise cv.Invalid(
            f"Hardware mode is not supported on '{board}'; use mode: {MODE_TIMER}"
        )
    return config


def validate_timer_config(config: ConfigType) -> ConfigType:
    if (
        (filter_us := config.get(CONF_FILTER)) is not None
        and (sample_us := config.get(CONF_SAMPLE_PERIOD)) is not None
        and filter_us.total_microseconds < sample_us.total_microseconds
    ):
        _LOGGER.warning(
            "beken_irda: filter is shorter than the sample period and will be clamped to one sample"
        )
    return config


BASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BekenIrdaComponent),
        cv.Optional(CONF_PIN, default="P26"): pins.internal_gpio_input_pin_schema,
        cv.Optional(CONF_TOLERANCE, default="30%"): remote_base.validate_tolerance,
        cv.Optional(CONF_DUMP, default=[]): remote_base.validate_dumpers,
        # Every remote_base trigger (on_nec, on_raw, ...); added here so both mode schemas share
        # them and the language schema still reports them.
        **{
            cv.Optional(key): validator
            for key, (_, validator) in remote_base.TRIGGER_REGISTRY.items()
        },
    }
).extend(cv.COMPONENT_SCHEMA)

TIMER_SCHEMA = BASE_SCHEMA.extend(
    {
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
).add_extra(validate_timer_config)

HARDWARE_SCHEMA = BASE_SCHEMA.extend(
    {
        cv.Optional(CONF_PIN, default="P26"): cv.All(
            pins.internal_gpio_input_pin_schema, validate_hardware_pin
        ),
    }
).add_extra(validate_hardware_board)

# The mode-specific options are separate schemas, so timer-only keys are rejected in hardware
# mode instead of being silently ignored.
CONFIG_SCHEMA = cv.typed_schema(
    {
        MODE_TIMER: TIMER_SCHEMA,
        MODE_HARDWARE: HARDWARE_SCHEMA,
    },
    key=CONF_MODE,
    default_type=MODE_TIMER,
    lower=True,
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
            f"use mode: {MODE_HARDWARE} or remove remote_transmitter"
        )


FINAL_VALIDATE_SCHEMA = final_validate


async def to_code(config: ConfigType) -> None:
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin)

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
