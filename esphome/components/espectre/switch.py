import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from . import CONF_ESPECTRE_ID, ESPectreComponent

DEPENDENCIES = ["espectre"]

CONF_SENSING = "sensing"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent),
            cv.Optional(CONF_SENSING): switch.switch_schema(
                switch.ApplySwitch,
                block_inverted=True,
                entity_category=ENTITY_CATEGORY_CONFIG,
                default_restore_mode="RESTORE_DEFAULT_ON",
                icon="mdi:radar",
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_SENSING),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ESPECTRE_ID])
    switches = switch.sub_switches(config, parent=hub)
    # The hub restores the state before the runtime starts and publishes it once applied.
    await switches(CONF_SENSING, hub.set_sensing_switch, apply="request_sensing(state)")
