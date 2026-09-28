import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_REFRESH
from esphome.types import ConfigType

from .. import CONF_LD6004_ID, LD6004Component, ld6004_ns

CONF_GENERATE_INTERFERENCE: str = "generate_interference"
CONF_CLEAR_INTERFERENCE: str = "clear_interference"
CONF_RESET_DETECTION: str = "reset_detection"
CONF_CLEAR_DWELL: str = "clear_dwell"
CONF_RESET_UNOCCUPIED: str = "reset_unoccupied"

DEPENDENCIES: list[str] = ["ld6004"]

LD6004Button: cg.MockObjClass = ld6004_ns.class_("LD6004Button", button.Button)
FIELDS: dict[str, int] = {
    CONF_REFRESH: 1,
    CONF_GENERATE_INTERFERENCE: 2,
    CONF_CLEAR_INTERFERENCE: 3,
    CONF_RESET_DETECTION: 4,
    CONF_CLEAR_DWELL: 5,
    CONF_RESET_UNOCCUPIED: 6,
}
CONFIG_SCHEMA: cv.Schema = cv.Schema(
    {
        cv.GenerateID(CONF_LD6004_ID): cv.use_id(LD6004Component),
        **{
            cv.Optional(key): button.button_schema(
                LD6004Button,
                entity_category="diagnostic" if key == CONF_REFRESH else "config",
            )
            for key in FIELDS
        },
    }
)


async def to_code(config: ConfigType) -> None:
    conf: ConfigType | None
    hub: cg.MockObj = await cg.get_variable(config[CONF_LD6004_ID])
    for key, index in FIELDS.items():
        if conf := config.get(key):
            await button.new_button(conf, hub, index)
