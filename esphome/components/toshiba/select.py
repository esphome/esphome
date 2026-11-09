import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import FILTER_SOURCE_FILES  # noqa: F401  pylint: disable=unused-import
from .climate import ToshibaRas25j2kvsgNdClimate

DEPENDENCIES = ["climate.toshiba"]

CONF_TOSHIBA_ID = "toshiba_id"
CONF_POWER_SELECTION = "power_selection"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TOSHIBA_ID): cv.use_id(ToshibaRas25j2kvsgNdClimate),
        cv.Required(CONF_POWER_SELECTION): select.select_schema(select.ApplySelect),
    }
)


async def to_code(config: ConfigType) -> None:
    var = await cg.get_variable(config[CONF_TOSHIBA_ID])
    selects = select.sub_selects(config, parent=var)
    await selects(
        CONF_POWER_SELECTION,
        var.set_power_selection_select,
        options=["100%", "75%", "50%", "Silent 1", "Silent 2"],
        apply="set_power_selection(index)",
    )
