from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
    CONF_OUTPUT,
)

DEPENDENCIES = []


sigma_delta_output_ns = cg.esphome_ns.namespace("sigma_delta_output")
SigmaDeltaOutput = sigma_delta_output_ns.class_(
    "SigmaDeltaOutput", output.FloatOutput, cg.PollingComponent
)

CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(cv.polling_component_schema("60s")).extend(
        {
            cv.Required(CONF_ID): cv.declare_id(SigmaDeltaOutput),
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        }
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    output_component = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(output_component))
