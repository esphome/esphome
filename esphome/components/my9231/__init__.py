from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BIT_DEPTH,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_NUM_CHIPS,
)
from esphome.types import ConfigType

AUTO_LOAD = ["output"]
DOMAIN = "my9231"
my9231_ns = cg.esphome_ns.namespace("my9231")
MY9231OutputComponent = my9231_ns.class_("MY9231OutputComponent", cg.Component)

MULTI_CONF = True
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(MY9231OutputComponent),
        cv.Required(CONF_DATA_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_CLOCK_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_NUM_CHANNELS, default=6): cv.int_range(min=3, max=1020),
        cv.Optional(CONF_NUM_CHIPS, default=2): cv.int_range(min=1, max=255),
        cv.Optional(CONF_BIT_DEPTH, default=16): cv.one_of(8, 12, 14, 16, int=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    # Duty values live in a static array sized from the config, so nothing is allocated at run time
    buf = f"{config[CONF_ID]}__pwm_amounts"
    num_channels = config[CONF_NUM_CHANNELS]
    cg.add_global(cg.RawStatement(f"static uint16_t {buf}[{num_channels}];"))
    var = cg.new_Pvariable(
        config[CONF_ID],
        await cg.gpio_pin_expression(config[CONF_DATA_PIN]),
        await cg.gpio_pin_expression(config[CONF_CLOCK_PIN]),
        cg.RawExpression(buf),
        num_channels,
        config[CONF_NUM_CHIPS],
        config[CONF_BIT_DEPTH],
    )
    await cg.register_component(var, config)
