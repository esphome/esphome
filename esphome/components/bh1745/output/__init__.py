import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from ..sensor import CONF_BH1745_ID, BH1745SComponent, bh1745_ns

BH1745InterruptPinOutput = bh1745_ns.class_(
    "BH1745InterruptPinOutput", output.BinaryOutput
)

CONFIG_SCHEMA = output.BINARY_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(BH1745InterruptPinOutput),
        cv.GenerateID(CONF_BH1745_ID): cv.use_id(BH1745SComponent),
    }
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    await cg.register_parented(var, config[CONF_BH1745_ID])
