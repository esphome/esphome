import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

from .. import CONF_WIJIBOARD_ID, WijiBoard, wijiboard_ns

DEPENDENCIES = ["wijiboard"]

WijiBoardText = wijiboard_ns.class_("WijiBoardText", text.Text, cg.Component)

CONFIG_SCHEMA = text.text_schema(WijiBoardText, mode="TEXT").extend(
    {
        cv.GenerateID(CONF_WIJIBOARD_ID): cv.use_id(WijiBoard),
    }
)


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_WIJIBOARD_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await cg.register_component(var, config)
    await text.register_text(var, config)
