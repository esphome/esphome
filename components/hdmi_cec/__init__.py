import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.const import CONF_ID

DEPENDENCIES = []

CONF_PIN = "pin"
CONF_ADDRESS = "address"
CONF_PROMISCUOUS_MODE = "promiscuous_mode"

hdmi_cec_ns = cg.esphome_ns.namespace("hdmi_cec")
HDMICEC = hdmi_cec_ns.class_(
    "HDMICEC", cg.Component
)

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(HDMICEC),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
        cv.Required(CONF_ADDRESS): cv.int_range(min=0, max=15),
        cv.Optional(CONF_PROMISCUOUS_MODE, False): cv.boolean
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cec_pin_ = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(cec_pin_))

    cg.add(var.set_address(config[CONF_ADDRESS]))
    cg.add(var.set_promiscuous_mode(config[CONF_PROMISCUOUS_MODE]))
