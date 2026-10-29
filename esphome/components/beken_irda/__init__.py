import logging
from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components import remote_base
from esphome.components.libretiny.const import KEY_BOARD, KEY_LIBRETINY
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
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

AUTO_LOAD = ["remote_base"]
CODEOWNERS = ["@bbbert"]
DEPENDENCIES = ["bk72xx"]
DOMAIN = "beken_irda"

CONF_SAMPLE_PERIOD = "sample_period"
CONF_TIMER_CHANNEL = "timer_channel"

MODE_HARDWARE = "hardware"
MODE_TIMER = "timer"

TYPE_PERCENTAGE = "percentage"
TYPE_TIME = "time"

beken_irda_ns = cg.esphome_ns.namespace("beken_irda")
remote_base_ns = cg.esphome_ns.namespace("remote_base")
ToleranceMode = remote_base_ns.enum("ToleranceMode")

BekenIrdaComponent = beken_irda_ns.class_(
    "BekenIrdaComponent", cg.Component, remote_base.RemoteReceiverBase
)

TOLERANCE_MODE = {
    TYPE_PERCENTAGE: ToleranceMode.TOLERANCE_MODE_PERCENTAGE,
    TYPE_TIME: ToleranceMode.TOLERANCE_MODE_TIME,
}

TOLERANCE_SCHEMA = cv.typed_schema(
    {
        TYPE_PERCENTAGE: cv.Schema(
            {cv.Required(CONF_VALUE): cv.All(cv.percentage_int, cv.uint32_t)}
        ),
        TYPE_TIME: cv.Schema(
            {
                cv.Required(CONF_VALUE): cv.All(
                    cv.positive_time_period_microseconds,
                    cv.Range(max=TimePeriod(microseconds=4294967295)),
                )
            }
        ),
    },
    lower=True,
    enum=TOLERANCE_MODE,
)

TIMER_ONLY_OPTIONS = (
    CONF_TIMER_CHANNEL,
    CONF_SAMPLE_PERIOD,
    CONF_FILTER,
    CONF_IDLE,
    CONF_BUFFER_SIZE,
)


@schema_extractor("schema")
def validate_tolerance(value: Any) -> ConfigType:
    if value is SCHEMA_EXTRACT:
        return TOLERANCE_SCHEMA

    if isinstance(value, dict):
        return TOLERANCE_SCHEMA(value)

    if "%" in str(value):
        type_ = TYPE_PERCENTAGE
    else:
        try:
            cv.positive_time_period_microseconds(value)
            type_ = TYPE_TIME
        except cv.Invalid as exc:
            raise cv.Invalid(
                "Tolerance must be a percentage or a time, for example '25%' or '500us'."
            ) from exc

    return TOLERANCE_SCHEMA(
        {
            CONF_VALUE: value,
            CONF_TYPE: type_,
        }
    )


def validate_config(config: ConfigType) -> ConfigType:
    if config[CONF_MODE] == MODE_HARDWARE:
        # The BK7252N/BK7253 IRDA block is a different, raw-timing peripheral and cannot use
        # the legacy NEC register set. Those boards currently report the BK7251 family, so the
        # board id is the only way to tell them apart.
        board = CORE.data.get(KEY_LIBRETINY, {}).get(KEY_BOARD, "").lower()
        if "bk7252n" in board or "bk7253" in board:
            raise cv.Invalid(
                f"Hardware mode is not supported on '{board}'; use mode: {MODE_TIMER}"
            )

        if config[CONF_PIN][CONF_NUMBER] != 26:
            raise cv.Invalid(
                "Hardware mode only accepts the IRDA pin P26; use mode: timer for other pins"
            )

        if ignored := [key for key in TIMER_ONLY_OPTIONS if key in config]:
            _LOGGER.warning(
                "beken_irda: ignoring timer-only options in hardware mode: %s",
                ", ".join(ignored),
            )
        return config

    if config.get(CONF_TIMER_CHANNEL) in (0, 2):
        _LOGGER.warning(
            "beken_irda: timer channel 0 is the RTOS tick and channel 2 is the SDK "
            "calibration timer; channel 1 is recommended"
        )

    if (
        (filter_us := config.get(CONF_FILTER)) is not None
        and (sample_us := config.get(CONF_SAMPLE_PERIOD)) is not None
        and filter_us.total_microseconds < sample_us.total_microseconds
    ):
        _LOGGER.warning(
            "beken_irda: filter is shorter than the sample period and will be clamped to one sample"
        )

    return config


CONFIG_SCHEMA = remote_base.validate_triggers(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BekenIrdaComponent),
            cv.Optional(CONF_PIN, default="P26"): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_MODE, default=MODE_TIMER): cv.one_of(
                MODE_TIMER, MODE_HARDWARE, lower=True
            ),
            cv.Optional(CONF_TIMER_CHANNEL): cv.int_range(min=0, max=2),
            cv.Optional(CONF_SAMPLE_PERIOD): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(
                    min=TimePeriod(microseconds=50),
                    max=TimePeriod(microseconds=2000),
                ),
            ),
            cv.Optional(CONF_TOLERANCE, default="30%"): validate_tolerance,
            cv.Optional(CONF_FILTER): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=4294967295)),
            ),
            cv.Optional(CONF_IDLE): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=4294967295)),
            ),
            cv.Optional(CONF_BUFFER_SIZE): cv.int_range(min=64, max=8192),
            cv.Optional(CONF_DUMP, default=[]): remote_base.validate_dumpers,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .add_extra(validate_config)
)


async def to_code(config: ConfigType) -> None:
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin)

    cg.add(var.set_hardware(config[CONF_MODE] == MODE_HARDWARE))
    cg.add_optional(var.set_timer_channel, config.get(CONF_TIMER_CHANNEL))
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
