import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_MODEL
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import FILTER_SOURCE_FILES  # noqa: F401  pylint: disable=unused-import
from .climate import MODEL_RAS_25J2KVSG_ND, ToshibaClimate

DEPENDENCIES = ["climate.toshiba"]

CONF_TOSHIBA_ID = "toshiba_id"
CONF_POWER_SELECTION = "power_selection"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TOSHIBA_ID): cv.use_id(ToshibaClimate),
        cv.Required(CONF_POWER_SELECTION): select.select_schema(select.ApplySelect),
    }
)


def _validate_model(config: ConfigType) -> ConfigType:
    climate = fv.get_declaring_config(config[CONF_TOSHIBA_ID])
    if climate[CONF_MODEL] != MODEL_RAS_25J2KVSG_ND:
        raise cv.Invalid(f"Power selection requires model {MODEL_RAS_25J2KVSG_ND}")
    return config


FINAL_VALIDATE_SCHEMA = _validate_model


async def to_code(config: ConfigType) -> None:
    var = await cg.get_variable(config[CONF_TOSHIBA_ID])
    selects = select.sub_selects(config, parent=var)
    await selects(
        CONF_POWER_SELECTION,
        var.set_power_selection_select,
        options=["100%", "75%", "50%", "Silent 1", "Silent 2"],
        apply="set_power_selection(index)",
    )
