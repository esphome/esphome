import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .climate import HitachiClimate

CONF_HITACHI_AC344_ID = "hitachi_ac344_id"
CONF_MILDEW_PROOF = "mildew_proof"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_HITACHI_AC344_ID): cv.use_id(HitachiClimate),
        cv.Required(CONF_MILDEW_PROOF): switch.switch_schema(
            switch.ApplySwitch,
            icon="mdi:shield-check-outline",
            entity_category=ENTITY_CATEGORY_CONFIG,
            default_restore_mode="RESTORE_DEFAULT_OFF",
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_HITACHI_AC344_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_MILDEW_PROOF, hub.set_mildew_proof_switch, apply="set_mildew_proof(state)"
    )
