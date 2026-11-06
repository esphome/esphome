import contextlib
import logging
from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt, remote_base
from esphome.components.libretiny import get_libretiny_family
from esphome.components.libretiny.const import FAMILY_BK7231Q, FAMILY_BK7238
from esphome.config_helpers import filter_source_files_from_platform
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_CARRIER_DUTY_PERCENT,
    CONF_CARRIER_FREQUENCY,
    CONF_CLOCK_RESOLUTION,
    CONF_DUMP,
    CONF_FILTER,
    CONF_ID,
    CONF_IDLE,
    CONF_PIN,
    CONF_RMT_SYMBOLS,
    CONF_TOLERANCE,
    CONF_TYPE,
    CONF_USE_DMA,
    CONF_VALUE,
    PLATFORM_BK72XX,
    PlatformFramework,
)
from esphome.core import CORE, TimePeriod
import esphome.final_validate as fv
from esphome.schema_extractors import SCHEMA_EXTRACT, schema_extractor
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

CONF_FILTER_SYMBOLS = "filter_symbols"
CONF_RECEIVE_SYMBOLS = "receive_symbols"
CONF_SAMPLE_PERIOD = "sample_period"
CONF_SAMPLING = "sampling"

SAMPLING_EDGE = "edge"
SAMPLING_TIMER = "timer"
DEFAULT_FILTER = "50us"
DEFAULT_SAMPLE_PERIOD = "100us"
# Below the 560us NEC mark, so a coarse sample period does not filter out every bit
MAX_TIMER_DEFAULT_FILTER_US = 400

AUTO_LOAD = ["remote_base"]
DOMAIN = "remote_receiver"
remote_receiver_ns = cg.esphome_ns.namespace("remote_receiver")
remote_base_ns = cg.esphome_ns.namespace("remote_base")

ToleranceMode = remote_base_ns.enum("ToleranceMode")

TYPE_PERCENTAGE = "percentage"
TYPE_TIME = "time"

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

RemoteReceiverComponent = remote_receiver_ns.class_(
    "RemoteReceiverComponent", remote_base.RemoteReceiverBase, cg.Component
)


def validate_config(config: ConfigType) -> ConfigType:
    if CORE.is_esp32:
        variant = esp32.get_esp32_variant()
        if variant in esp32_rmt.VARIANTS_NO_RMT:
            return config
        if variant in (esp32.VARIANT_ESP32, esp32.VARIANT_ESP32S2):
            max_idle = 65535
        else:
            max_idle = 32767
        if CONF_CLOCK_RESOLUTION in config:
            max_idle = int(max_idle * 1000000 / config[CONF_CLOCK_RESOLUTION])
        if config[CONF_IDLE].total_microseconds > max_idle:
            raise cv.Invalid(f"config 'idle' exceeds the maximum value of {max_idle}us")
    return config


def validate_tolerance(value: Any) -> ConfigType:
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
                "Tolerance must be a percentage or time. Configurations made before 2024.5.0 treated the value as a percentage."
            ) from exc

    return TOLERANCE_SCHEMA(
        {
            CONF_VALUE: value,
            CONF_TYPE: type_,
        }
    )


def timer_filter_us(sample_us: int) -> int:
    """1.5 sample periods, so a one-sample glitch is still removed."""
    return min(sample_us * 3 // 2, MAX_TIMER_DEFAULT_FILTER_US)


def timer_sampling_defaults(config: ConfigType) -> ConfigType:
    """Fill the timer defaults before the schema, whose filter default is for edge sampling."""
    if (
        not isinstance(config, dict)
        or str(config.get(CONF_SAMPLING)).lower() != SAMPLING_TIMER
    ):
        return config
    config = {CONF_SAMPLE_PERIOD: DEFAULT_SAMPLE_PERIOD, **config}
    if CONF_FILTER not in config:
        # an invalid sample_period is reported by the schema
        with contextlib.suppress(cv.Invalid):
            sample = cv.positive_time_period_microseconds(config[CONF_SAMPLE_PERIOD])
            config[CONF_FILTER] = f"{timer_filter_us(sample.total_microseconds)}us"
    return config


def validate_sampling(config: ConfigType) -> ConfigType:
    if config.get(CONF_SAMPLING) != SAMPLING_TIMER:
        if CONF_SAMPLE_PERIOD in config:
            raise cv.Invalid(
                f"'{CONF_SAMPLE_PERIOD}' needs '{CONF_SAMPLING}: {SAMPLING_TIMER}'"
            )
        return config
    # The BK7231Q SDK registers no timer driver
    if get_libretiny_family() == FAMILY_BK7231Q:
        raise cv.Invalid(
            f"'{CONF_SAMPLING}: {SAMPLING_TIMER}' is not available on {FAMILY_BK7231Q}"
        )
    sample_us = config[CONF_SAMPLE_PERIOD].total_microseconds
    filter_us = config[CONF_FILTER].total_microseconds
    if filter_us <= sample_us:
        _LOGGER.warning(
            "filter is not longer than the sample period, so it cannot remove one-sample glitches"
        )
    # Both are counted in 26MHz timer ticks
    for key in (CONF_FILTER, CONF_IDLE):
        if config[key].total_microseconds > 1000000:
            raise cv.Invalid(
                f"'{key}' can be at most 1s with '{CONF_SAMPLING}: {SAMPLING_TIMER}'"
            )
    return config


MULTI_CONF = True
_RECEIVER_SCHEMA = remote_base.validate_triggers(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RemoteReceiverComponent),
            cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Optional(CONF_DUMP, default=[]): remote_base.validate_dumpers,
            cv.Optional(CONF_TOLERANCE, default="25%"): validate_tolerance,
            # pulse ring targets hold one 4 byte entry per pulse; 4000b keeps their 1000 pulses
            cv.SplitDefault(
                CONF_BUFFER_SIZE,
                esp32=cv.UNDEFINED,
                # the pulse ring needs a size; only RMT targets size themselves in setup()
                **{
                    f"esp32_{variant.removeprefix('ESP32').lower()}": "4000b"
                    for variant in esp32_rmt.VARIANTS_NO_RMT
                },
                esp8266="4000b",
                bk72xx="4000b",
                ln882x="4000b",
                rtl87xx="4000b",
                rp2="4000b",
            ): cv.All(cv.validate_bytes, cv.int_range(min=64, max=65535)),
            # timer_sampling_defaults fills the timer default
            cv.Optional(CONF_FILTER, default=DEFAULT_FILTER): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=4294967295)),
            ),
            cv.Optional(CONF_SAMPLING): cv.All(
                cv.only_on(PLATFORM_BK72XX),
                cv.one_of(SAMPLING_EDGE, SAMPLING_TIMER, lower=True),
            ),
            # A tick counter that wraps every 2ms measures the time between samples
            cv.Optional(CONF_SAMPLE_PERIOD): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(
                    min=TimePeriod(microseconds=50), max=TimePeriod(microseconds=1000)
                ),
            ),
            cv.Optional(CONF_CLOCK_RESOLUTION): cv.All(
                cv.only_on_esp32,
                esp32_rmt.validate_clock_resolution(),
            ),
            cv.Optional(CONF_IDLE, default="10ms"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=4294967295)),
            ),
            cv.SplitDefault(
                CONF_RMT_SYMBOLS,
                esp32=192,
                esp32_c2=cv.UNDEFINED,
                esp32_c3=96,
                esp32_c5=96,
                esp32_c6=96,
                esp32_c61=cv.UNDEFINED,
                esp32_h2=96,
                esp32_h4=96,
                esp32_h21=96,
                esp32_p4=192,
                esp32_s2=192,
                esp32_s3=192,
                esp32_s31=192,
            ): cv.All(cv.only_on_esp32, cv.int_range(min=2)),
            cv.Optional(CONF_FILTER_SYMBOLS): cv.All(
                cv.only_on_esp32, cv.int_range(min=0)
            ),
            cv.SplitDefault(
                CONF_RECEIVE_SYMBOLS,
                esp32=192,
                esp32_c2=cv.UNDEFINED,
                esp32_c61=cv.UNDEFINED,
            ): cv.All(cv.only_on_esp32, cv.int_range(min=2)),
            cv.Optional(CONF_USE_DMA): cv.All(
                esp32.only_on_variant(
                    supported=[esp32.VARIANT_ESP32P4, esp32.VARIANT_ESP32S3]
                ),
                cv.boolean,
            ),
            cv.SplitDefault(
                CONF_CARRIER_DUTY_PERCENT,
                esp32=100,
                esp32_c2=cv.UNDEFINED,
                esp32_c61=cv.UNDEFINED,
            ): cv.All(
                cv.only_on_esp32,
                cv.percentage_int,
                cv.Range(min=1, max=100),
            ),
            cv.SplitDefault(
                CONF_CARRIER_FREQUENCY,
                esp32="0Hz",
                esp32_c2=cv.UNDEFINED,
                esp32_c61=cv.UNDEFINED,
            ): cv.All(cv.only_on_esp32, cv.frequency, cv.int_),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .add_extra(
        esp32_rmt.validate_rmt_not_supported(
            [
                CONF_CLOCK_RESOLUTION,
                CONF_USE_DMA,
                CONF_RMT_SYMBOLS,
                CONF_FILTER_SYMBOLS,
                CONF_RECEIVE_SYMBOLS,
                CONF_CARRIER_DUTY_PERCENT,
                CONF_CARRIER_FREQUENCY,
            ]
        )
    )
    .add_extra(validate_config)
    .add_extra(validate_sampling)
    # setup_priority::DATA, the default of Component::get_setup_priority()
    .add_extra(esp32_rmt.request_rmt_channel("RX", 600.0))
)


@schema_extractor("triggers")
def CONFIG_SCHEMA(config: ConfigType) -> ConfigType:
    """Fill the timer sampling defaults before the schema, which then sees them as set."""
    if config == SCHEMA_EXTRACT:
        return _RECEIVER_SCHEMA(SCHEMA_EXTRACT)
    return _RECEIVER_SCHEMA(timer_sampling_defaults(config))


def final_validate_sampling(config: ConfigType) -> ConfigType:
    if config.get(CONF_SAMPLING) != SAMPLING_TIMER:
        return config
    full_config = fv.full_config.get()
    # The backend is chosen for the whole build and owns BKTIMER1
    if len(full_config[DOMAIN]) > 1:
        raise cv.Invalid(
            f"'{CONF_SAMPLING}: {SAMPLING_TIMER}' switches every remote_receiver in the build to the "
            "timer backend, which supports only one remote_receiver"
        )
    # On BK7238 remote_transmitter always paces its PWM envelope with BKTIMER1
    if get_libretiny_family() == FAMILY_BK7238 and "remote_transmitter" in full_config:
        raise cv.Invalid(
            f"'{CONF_SAMPLING}: {SAMPLING_TIMER}' and remote_transmitter both use BKTIMER1 on {FAMILY_BK7238}"
        )
    return config


FINAL_VALIDATE_SCHEMA = cv.All(
    final_validate_sampling, esp32_rmt.final_validate_rmt_channels
)


async def to_code(config: ConfigType) -> None:
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    if CORE.is_esp32 and esp32.get_esp32_variant() not in esp32_rmt.VARIANTS_NO_RMT:
        # Re-enable ESP-IDF's RMT driver (excluded by default to save compile time)
        esp32.include_builtin_idf_component("esp_driver_rmt")

        var = cg.new_Pvariable(config[CONF_ID], pin)
        cg.add(var.set_rmt_symbols(config[CONF_RMT_SYMBOLS]))
        cg.add(var.set_receive_symbols(config[CONF_RECEIVE_SYMBOLS]))
        cg.add_optional(var.set_with_dma, config.get(CONF_USE_DMA))
        cg.add_optional(var.set_clock_resolution, config.get(CONF_CLOCK_RESOLUTION))
        cg.add_optional(var.set_filter_symbols, config.get(CONF_FILTER_SYMBOLS))
        cg.add(var.set_carrier_duty_percent(config[CONF_CARRIER_DUTY_PERCENT]))
        cg.add(var.set_carrier_frequency(config[CONF_CARRIER_FREQUENCY]))
    else:
        var = cg.new_Pvariable(config[CONF_ID], pin)

    dumpers = await remote_base.build_dumpers(config[CONF_DUMP])
    for dumper in dumpers:
        remote_base.add_dumper(var, dumper)

    triggers = await remote_base.build_triggers(config)
    for trigger in triggers:
        remote_base.add_listener(var, trigger)
    await cg.register_component(var, config)
    esp32_rmt.register_rmt_channel(config)

    cg.add(
        var.set_tolerance(
            config[CONF_TOLERANCE][CONF_VALUE], config[CONF_TOLERANCE][CONF_TYPE]
        )
    )
    cg.add(var.set_filter_us(config[CONF_FILTER]))
    cg.add(var.set_idle_us(config[CONF_IDLE]))
    if config.get(CONF_SAMPLING) != SAMPLING_TIMER:
        cg.add_optional(var.set_buffer_size, config.get(CONF_BUFFER_SIZE))
    else:
        cg.add_define("USE_REMOTE_RECEIVER_TIMER_SAMPLING")
        cg.add(var.set_sample_period_us(config[CONF_SAMPLE_PERIOD]))
        # one frame half captures while the loop dispatches the other; the 65535 byte
        # buffer_size cap keeps frame_entries within the store's uint16_t
        frame_entries = config[CONF_BUFFER_SIZE] // 4 // 2
        frames_id = cg.unique_global_id(f"{config[CONF_ID].id}__frames", cg.int32)
        frames = cg.MockObj(frames_id, ".")
        CORE.register_variable(frames_id, frames)
        cg.add_global(
            cg.RawStatement(f"static int32_t {frames_id}[{frame_entries * 2}];")
        )
        cg.add(var.set_frame_buffer(frames, frame_entries))


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "remote_receiver_rmt.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "remote_receiver_bk72xx.cpp": {PlatformFramework.BK72XX_ARDUINO},
        "remote_receiver.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
            PlatformFramework.ESP8266_ARDUINO,
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
            PlatformFramework.RP2_ARDUINO,
        },
    }
)
