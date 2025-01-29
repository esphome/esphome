from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32_rmt, remote_base
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_CLOCK_DIVIDER,
    CONF_CLOCK_RESOLUTION,
    CONF_DUMP,
    CONF_FILTER,
    CONF_ID,
    CONF_IDLE,
    CONF_MEMORY_BLOCKS,
    CONF_PIN,
    CONF_RMT_CHANNEL,
    CONF_RMT_SYMBOLS,
    CONF_TOLERANCE,
    CONF_TYPE,
    CONF_USE_DMA,
    CONF_VALUE,
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
                rtl87xx="1000b",
            ): cv.validate_bytes,
            cv.Optional(CONF_FILTER, default="50us"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=4294967295)),
            ),
            cv.SplitDefault(CONF_CLOCK_DIVIDER, esp32_arduino=80): cv.All(
                cv.only_on_esp32,
                cv.only_with_arduino,
                cv.int_range(min=1, max=255),
            ),
            cv.Optional(CONF_CLOCK_RESOLUTION): cv.All(
                cv.only_on_esp32,
                cv.only_with_esp_idf,
                esp32_rmt.validate_clock_resolution(),
            ),
            cv.Optional(CONF_IDLE, default="10ms"): cv.All(
                cv.positive_time_period_microseconds,
                cv.Range(max=TimePeriod(microseconds=4294967295)),
            ),
            cv.SplitDefault(CONF_MEMORY_BLOCKS, esp32_arduino=3): cv.All(
                cv.only_with_arduino, cv.int_range(min=1, max=8)
            ),
            cv.Optional(CONF_RMT_CHANNEL): cv.All(
                cv.only_with_arduino, esp32_rmt.validate_rmt_channel(tx=False)
            ),
            cv.SplitDefault(
                CONF_RMT_SYMBOLS,
                esp32_idf=192,
                esp32_s2_idf=192,
                esp32_s3_idf=192,
                esp32_c3_idf=96,
                esp32_c6_idf=96,
                esp32_h2_idf=96,
            ): cv.All(cv.only_with_esp_idf, cv.int_range(min=2)),
            cv.Optional(CONF_FILTER_SYMBOLS): cv.All(
                cv.only_with_esp_idf, cv.int_range(min=0)
            ),
            cv.SplitDefault(
                CONF_RECEIVE_SYMBOLS,
                esp32_idf=192,
            ): cv.All(cv.only_with_esp_idf, cv.int_range(min=2)),
            cv.Optional(CONF_USE_DMA): cv.All(cv.only_with_esp_idf, cv.boolean),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    if CORE.is_esp32:
        if esp32_rmt.use_new_rmt_driver():
            var = cg.new_Pvariable(config[CONF_ID], pin)
            cg.add(var.set_rmt_symbols(config[CONF_RMT_SYMBOLS]))
            cg.add(var.set_receive_symbols(config[CONF_RECEIVE_SYMBOLS]))
            if CONF_USE_DMA in config:
                cg.add(var.set_with_dma(config[CONF_USE_DMA]))
            if CONF_CLOCK_RESOLUTION in config:
                cg.add(var.set_clock_resolution(config[CONF_CLOCK_RESOLUTION]))
            if CONF_FILTER_SYMBOLS in config:
                cg.add(var.set_filter_symbols(config[CONF_FILTER_SYMBOLS]))
        else:
            if (rmt_channel := config.get(CONF_RMT_CHANNEL, None)) is not None:
                var = cg.new_Pvariable(
                    config[CONF_ID], pin, rmt_channel, config[CONF_MEMORY_BLOCKS]
                )
            else:
                var = cg.new_Pvariable(config[CONF_ID], pin, config[CONF_MEMORY_BLOCKS])
            cg.add(var.set_clock_divider(config[CONF_CLOCK_DIVIDER]))
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
