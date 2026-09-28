import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.core.entity_helpers import SubEntities
from esphome.types import ConfigType

from .. import CONF_LD6004_ID, LD6004Component, ld6004_ns

CONF_TARGET_OUTPUT: str = "target_output"

DEPENDENCIES: list[str] = ["ld6004"]

LD6004Switch: cg.MockObjClass = ld6004_ns.class_("LD6004Switch", switch.Switch)
CONFIG_SCHEMA: cv.Schema = cv.Schema(
    {
        cv.GenerateID(CONF_LD6004_ID): cv.use_id(LD6004Component),
        cv.Optional(CONF_TARGET_OUTPUT): switch.switch_schema(
            LD6004Switch, entity_category="config"
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_LD6004_SWITCH")
    hub: cg.MockObj = await cg.get_variable(config[CONF_LD6004_ID])
    switches: SubEntities = switch.sub_switches(config)
    await switches(CONF_TARGET_OUTPUT, hub.set_target_output_switch, hub)
