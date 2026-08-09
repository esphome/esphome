import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.types import ConfigType

from ..climate import HitachiClimate, hitachi_ac344_ns

CONF_HITACHI_AC344_ID = "hitachi_ac344_id"
CONF_MILDEW_PROOF = "mildew_proof"

MildewProofSwitch = hitachi_ac344_ns.class_("MildewProofSwitch", switch.Switch)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HITACHI_AC344_ID): cv.use_id(HitachiClimate),
        cv.Required(CONF_MILDEW_PROOF): switch.switch_schema(
            MildewProofSwitch,
            icon="mdi:shield-check-outline",
            default_restore_mode="DISABLED",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_HITACHI_AC344_MILDEW_PROOF")
    parent = await cg.get_variable(config[CONF_HITACHI_AC344_ID])
    var = await switch.new_switch(config[CONF_MILDEW_PROOF])
    await cg.register_parented(var, parent)
    cg.add(parent.set_mildew_proof_switch(var))
