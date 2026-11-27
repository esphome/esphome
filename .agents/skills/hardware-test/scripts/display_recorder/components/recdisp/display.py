import esphome.codegen as cg
from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

recdisp_ns = cg.esphome_ns.namespace("recdisp")
RecDisplay = recdisp_ns.class_("RecDisplay", display.Display)

CONFIG_SCHEMA = display.FULL_DISPLAY_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(RecDisplay)}
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
