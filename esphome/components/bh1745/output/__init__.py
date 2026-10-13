import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import CONF_BH1745_ID, BH1745Component, bh1745_ns

DEPENDENCIES = ["bh1745"]

BH1745InterruptPinOutput = bh1745_ns.class_(
    "BH1745InterruptPinOutput", output.BinaryOutput
)

CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BH1745InterruptPinOutput),
        cv.GenerateID(CONF_BH1745_ID): cv.use_id(BH1745Component),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    hub = await cg.get_variable(config[CONF_BH1745_ID])
    await cg.register_parented(var, hub)
    cg.add(hub.enable_interrupt_output())
