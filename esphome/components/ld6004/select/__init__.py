import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_SENSITIVITY
from esphome.types import ConfigType

from .. import CONF_LD6004_ID, LD6004Component, ld6004_ns

CONF_TRIGGER_SPEED: str = "trigger_speed"
CONF_INSTALLATION_MODE: str = "installation_mode"
CONF_WORK_MODE: str = "work_mode"
CONF_P20_MODE: str = "p20_mode"

DEPENDENCIES: list[str] = ["ld6004"]

LD6004Select: cg.MockObjClass = ld6004_ns.class_("LD6004Select", select.Select)
FIELDS: dict[str, list[str]] = {
    CONF_SENSITIVITY: ["Low", "Medium", "High"],
    CONF_TRIGGER_SPEED: ["Slow", "Medium", "Fast"],
    CONF_INSTALLATION_MODE: ["Top", "Side"],
    CONF_WORK_MODE: [
        "Normal",
        "Low power",
        "Radar off P20 high",
        "Radar off P20 low",
        "High reflectivity",
    ],
    CONF_P20_MODE: [
        "Presence high",
        "Presence low",
        "Constant low",
        "Constant high",
        "Pulse low",
        "Pulse high",
    ],
}
CONFIG_SCHEMA: cv.Schema = cv.Schema(
    {
        cv.GenerateID(CONF_LD6004_ID): cv.use_id(LD6004Component),
        **{
            cv.Optional(key): select.select_schema(
                LD6004Select, entity_category="config"
            )
            for key in FIELDS
        },
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LD6004_SELECT")
    conf: ConfigType | None
    hub: cg.MockObj = await cg.get_variable(config[CONF_LD6004_ID])
    for index, key in enumerate(FIELDS):
        if conf := config.get(key):
            entity: cg.MockObj = await select.new_select(
                conf, hub, index, options=FIELDS[key]
            )
            cg.add(hub.set_select_entity(index, entity))
