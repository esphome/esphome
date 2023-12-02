import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, output
from esphome.const import CONF_ID, CONF_OUTPUT, CONF_DURATION
from .. import output_ns

OutputButton = output_ns.class_("OutputButton", button.Button, cg.Component)

CONFIG_SCHEMA = (
    button.button_schema(OutputButton)
    .extend(
        {
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
            cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_duration(config[CONF_DURATION]))

    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(output_))

    await cg.register_component(var, config)
    await button.register_button(var, config)
