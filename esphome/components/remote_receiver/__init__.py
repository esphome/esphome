from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt, remote_base
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
    PlatformFramework,
)
from esphome.core import CORE, TimePeriod

CONF_FILTER_SYMBOLS = "filter_symbols"
CONF_RECEIVE_SYMBOLS = "receive_symbols"

AUTO_LOAD = ["remote_base"]
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


def validate_config(config):
    if CORE.is_esp32:
        variant = esp32.get_esp32_variant()
        if variant in (esp32.VARIANT_ESP32, esp32.VARIANT_ESP32S2):
            max_idle = 65535
        else:
            max_idle = 32767
        if CONF_CLOCK_RESOLUTION in config:
            max_idle = int(max_idle * 1000000 / config[CONF_CLOCK_RESOLUTION])
        if config[CONF_IDLE].total_microseconds > max_idle:
            raise cv.Invalid(f"config 'idle' exceeds the maximum value of {max_idle}us")
    return config


def validate_tolerance(value):
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


MULTI_CONF = True
CONFIG_SCHEMA = remote_base.validate_triggers(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RemoteReceiverComponent),
            cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Optional(CONF_DUMP, default=[]): remote_base.validate_dumpers,
            cv.Optional(CONF_TOLERANCE, default="25%"): validate_tolerance,
            cv.SplitDefault(
                CONF_BUFFER_SIZE,
                esp32="10000b",
                esp8266="1000b",
                bk72xx="1000b",
                ln882x="1000b",
                rtl87xx="1000b",
                rp2040="1000b",
            ): cv.validate_bytes,
            cv.Optional(CONF_FILTER, default="50us"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=4294967295)),
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
                esp32_c3=96,
                esp32_c5=96,
                esp32_c6=96,
                esp32_h2=96,
                esp32_p4=192,
                esp32_s2=192,
                esp32_s3=192,
            ): cv.All(cv.only_on_esp32, cv.int_range(min=2)),
            cv.Optional(CONF_FILTER_SYMBOLS): cv.All(
                cv.only_on_esp32, cv.int_range(min=0)
            ),
            cv.SplitDefault(
                CONF_RECEIVE_SYMBOLS,
                esp32=192,
            ): cv.All(cv.only_on_esp32, cv.int_range(min=2)),
            cv.Optional(CONF_USE_DMA): cv.All(
                esp32.only_on_variant(
                    supported=[esp32.VARIANT_ESP32P4, esp32.VARIANT_ESP32S3]
                ),
                cv.boolean,
            ),
            cv.SplitDefault(CONF_CARRIER_DUTY_PERCENT, esp32=100): cv.All(
                cv.only_on_esp32,
                cv.percentage_int,
                cv.Range(min=1, max=100),
            ),
            cv.SplitDefault(CONF_CARRIER_FREQUENCY, esp32="0Hz"): cv.All(
                cv.only_on_esp32, cv.frequency, cv.int_
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .add_extra(validate_config)
)


async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    if CORE.is_esp32:
        var = cg.new_Pvariable(config[CONF_ID], pin)
        cg.add(var.set_rmt_symbols(config[CONF_RMT_SYMBOLS]))
        cg.add(var.set_receive_symbols(config[CONF_RECEIVE_SYMBOLS]))
        if CONF_USE_DMA in config:
            cg.add(var.set_with_dma(config[CONF_USE_DMA]))
        if CONF_CLOCK_RESOLUTION in config:
            cg.add(var.set_clock_resolution(config[CONF_CLOCK_RESOLUTION]))
        if CONF_FILTER_SYMBOLS in config:
            cg.add(var.set_filter_symbols(config[CONF_FILTER_SYMBOLS]))
        cg.add(var.set_carrier_duty_percent(config[CONF_CARRIER_DUTY_PERCENT]))
        cg.add(var.set_carrier_frequency(config[CONF_CARRIER_FREQUENCY]))
    else:
        var = cg.new_Pvariable(config[CONF_ID], pin)

    dumpers = await remote_base.build_dumpers(config[CONF_DUMP])
    for dumper in dumpers:
        cg.add(var.register_dumper(dumper))

    triggers = await remote_base.build_triggers(config)
    for trigger in triggers:
        cg.add(var.register_listener(trigger))
    await cg.register_component(var, config)

    cg.add(
        var.set_tolerance(
            config[CONF_TOLERANCE][CONF_VALUE], config[CONF_TOLERANCE][CONF_TYPE]
        )
    )
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_filter_us(config[CONF_FILTER]))
    cg.add(var.set_idle_us(config[CONF_IDLE]))


FILTER_SOURCE_FILES = filter_source_files_from_platform(
    {
        "remote_receiver_esp32.cpp": {
            PlatformFramework.ESP32_ARDUINO,
            PlatformFramework.ESP32_IDF,
        },
        "remote_receiver.cpp": {
            PlatformFramework.ESP8266_ARDUINO,
            PlatformFramework.BK72XX_ARDUINO,
            PlatformFramework.RTL87XX_ARDUINO,
            PlatformFramework.LN882X_ARDUINO,
            PlatformFramework.RP2040_ARDUINO,
        },
    }
)
