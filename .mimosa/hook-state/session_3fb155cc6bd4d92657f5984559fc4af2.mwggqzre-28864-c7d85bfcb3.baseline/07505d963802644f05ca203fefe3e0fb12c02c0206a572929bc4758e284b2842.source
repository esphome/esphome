import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

AUTO_LOAD = ["gpio_expander"]

gpio_expander_test_component_ns = cg.esphome_ns.namespace(
    "gpio_expander_test_component"
)

GPIOExpanderTestComponent = gpio_expander_test_component_ns.class_(
    "GPIOExpanderTestComponent", cg.Component
)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GPIOExpanderTestComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
