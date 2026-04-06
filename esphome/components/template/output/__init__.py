from esphome import automation
import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_BINARY, CONF_ID, CONF_TYPE

from .. import template_ns

TemplateBinaryOutput = template_ns.class_("TemplateBinaryOutput", output.BinaryOutput)
TemplateFloatOutput = template_ns.class_("TemplateFloatOutput", output.FloatOutput)

CONF_FLOAT = "float"
CONF_WRITE_ACTION = "write_action"

CONFIG_SCHEMA = cv.typed_schema(
    {
        CONF_BINARY: output.BINARY_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(TemplateBinaryOutput),
                cv.Required(CONF_WRITE_ACTION): automation.validate_automation(
                    single=True
                ),
            }
        ),
        CONF_FLOAT: output.FLOAT_OUTPUT_SCHEMA.extend(
            {
                cv.GenerateID(): cv.declare_id(TemplateFloatOutput),
                cv.Required(CONF_WRITE_ACTION): automation.validate_automation(
                    single=True
                ),
            }
        ),
    },
    lower=True,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    if config[CONF_TYPE] == CONF_BINARY:
        await automation.build_automation(
            var.get_trigger(), [(bool, "state")], config[CONF_WRITE_ACTION]
        )
    else:
        await automation.build_automation(
            var.get_trigger(), [(float, "state")], config[CONF_WRITE_ACTION]
        )
    await output.register_output(var, config)
