import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output, lock
from esphome.const import CONF_ID, CONF_OUTPUT
from .. import output_ns

OutputLock = output_ns.class_("OutputLock", lock.Lock, cg.Component)

CONFIG_SCHEMA = lock.LOCK_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(OutputLock),
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await lock.register_lock(var, config)

    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(output_))
