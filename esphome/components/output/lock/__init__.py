import esphome.codegen as cg
from esphome.components import lock, output
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT

from .. import output_ns

OutputLock = output_ns.class_("OutputLock", lock.Lock, cg.Component)

CONFIG_SCHEMA = (
    lock.lock_schema(OutputLock)
    .extend(
        {
            cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await lock.new_lock(config)
    await cg.register_component(var, config)

    await cg.add_variable(var.set_output, config[CONF_OUTPUT])
