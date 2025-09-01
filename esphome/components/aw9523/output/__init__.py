import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN

from .. import CONF_AW9523, AW9523Component, aw9523_ns

CODEOWNERS = ["@beormund"]
CONF_MAX_CURRENT = "max_current"
DEPENDENCIES = ["aw9523"]

AW9523FloatOutputChannel = aw9523_ns.class_(
    "AW9523FloatOutputChannel", output.FloatOutput, cg.Component
)

CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(AW9523FloatOutputChannel),
        cv.Required(CONF_AW9523): cv.use_id(AW9523Component),
        cv.Required(CONF_PIN): cv.int_range(min=0, max=15),
        cv.Optional(CONF_MAX_CURRENT, default=10): cv.All(
            cv.float_with_unit("current", "(mA)", optional_unit=True),
            cv.float_range(min=0, max=37),
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_AW9523])
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)
    cg.add(var.set_max_current(config[CONF_MAX_CURRENT]))
    cg.add(var.set_pin(config[CONF_PIN]))
    cg.add(var.set_parent(parent))
