from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_FILTER, CONF_ID, CONF_PIN
from esphome.core import TimePeriod
from esphome.types import ConfigType

CODEOWNERS = ["@Bofr5555"]
DOMAIN = "vevor_dtx"

CONF_BIT_TIME = "bit_time"
CONF_MAX_GAP = "max_gap"
CONF_VEVOR_DTX_ID = "vevor_dtx_id"

vevor_dtx_ns = cg.esphome_ns.namespace("vevor_dtx")
VevorDtxComponent = vevor_dtx_ns.class_("VevorDtxComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(VevorDtxComponent),
        cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema),
        cv.Optional(CONF_BIT_TIME, default="88us"): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(
                min=TimePeriod(microseconds=50),
                max=TimePeriod(microseconds=150),
            ),
        ),
        cv.Optional(CONF_MAX_GAP, default="2000us"): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(
                min=TimePeriod(microseconds=500),
                max=TimePeriod(microseconds=20000),
            ),
        ),
        cv.Optional(CONF_FILTER, default="20us"): cv.All(
            cv.positive_time_period_microseconds,
            cv.Range(
                min=TimePeriod(microseconds=0),
                max=TimePeriod(microseconds=500),
            ),
        ),
        cv.Optional(CONF_BUFFER_SIZE, default="512b"): cv.All(
            cv.validate_bytes,
            cv.Range(min=128, max=2048),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    cg.add(var.set_bit_time_us(config[CONF_BIT_TIME]))
    cg.add(var.set_max_gap_us(config[CONF_MAX_GAP]))
    cg.add(var.set_filter_us(config[CONF_FILTER]))
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
