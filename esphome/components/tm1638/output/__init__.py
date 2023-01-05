import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output
from esphome.const import CONF_ID, CONF_LED
from ..display import tm1638_ns, TM1638Component, CONF_TM1638_ID

TM1638OutputLed = tm1638_ns.class_("TM1638OutputLed", output.BinaryOutput, cg.Component)


CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TM1638OutputLed),
        cv.GenerateID(CONF_TM1638_ID): cv.use_id(TM1638Component),
        cv.Required(CONF_LED): cv.int_range(min=0, max=7),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    await cg.register_component(var, config)
    cg.add(var.set_lednum(config[CONF_LED]))
    hub = await cg.get_variable(config[CONF_TM1638_ID])
    cg.add(var.set_tm1638(hub))
