import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID, CONF_PIN_A, CONF_PIN_B, CONF_ENABLE_PIN
from .. import hbridge_ns, HBRIDGE_CONFIG_SCHEMA

CODEOWNERS = ["@DotNetDann"]
AUTO_LOAD = ["hbridge"]

HBridgeLightOutput = hbridge_ns.class_(
    "HBridgeLightOutput", cg.PollingComponent, light.LightOutput
)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HBridgeLightOutput),
    }
).extend(HBRIDGE_CONFIG_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    # HBridge driver config
    pina = await cg.get_variable(config[CONF_PIN_A])
    cg.add(var.set_hbridge_pin_a(pina))
    pinb = await cg.get_variable(config[CONF_PIN_B])
    cg.add(var.set_hbridge_pin_b(pinb))

    if CONF_ENABLE_PIN in config:
        pin_enable = await cg.get_variable(config[CONF_ENABLE_PIN])
        cg.add(var.set_hbridge_enable_pin(pin_enable))

