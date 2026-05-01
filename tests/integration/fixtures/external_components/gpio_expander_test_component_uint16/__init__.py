import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["gpio_expander"]

gpio_expander_test_component_uint16_ns = cg.esphome_ns.namespace(
    "gpio_expander_test_component_uint16"
)

GPIOExpanderTestUint16Component = gpio_expander_test_component_uint16_ns.class_(
    "GPIOExpanderTestUint16Component", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GPIOExpanderTestUint16Component),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
