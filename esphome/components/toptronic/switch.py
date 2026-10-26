import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_INITIAL_STATE, CONF_RESTORE_MODE, CONF_RESTORE_STATE
from esphome.types import ConfigType

from . import CONF_TOPTRONIC_ID, TopTronicComponent

CONF_CANDUMP = "candump"
CONF_FIND_CAN_ID = "find_can_id"

# Each switch turns one build-wide CAN traffic log on or off; both start off at boot
# and turn themselves off after two minutes, so every other restore setting would be
# silently ignored
_DEBUG_SWITCH_SCHEMA = switch.switch_schema(
    switch.ApplySwitch, default_restore_mode="ALWAYS_OFF"
).extend(
    {
        cv.Optional(CONF_RESTORE_MODE): cv.one_of("ALWAYS_OFF", upper=True),
        cv.Optional(CONF_RESTORE_STATE): cv.invalid(
            "The debug logs always start off, so 'restore_state' is not supported"
        ),
        cv.Optional(CONF_INITIAL_STATE): cv.invalid(
            "The debug logs always start off, so 'initial_state' is not supported"
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TOPTRONIC_ID): cv.use_id(TopTronicComponent),
        # Log every CAN frame sent or received, with the "candump" tag
        cv.Optional(CONF_CANDUMP): _DEBUG_SWITCH_SCHEMA,
        # Log the sender of every single-frame GET (0x40) and RESPONSE (0x42) message at warning level
        cv.Optional(CONF_FIND_CAN_ID): _DEBUG_SWITCH_SCHEMA,
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TOPTRONIC_ID])
    switches = switch.sub_switches(config, parent=hub)
    await switches(
        CONF_CANDUMP, hub.set_candump_switch, apply="set_candump_enabled(state)"
    )
    await switches(
        CONF_FIND_CAN_ID,
        hub.set_find_can_id_switch,
        apply="set_find_can_id_enabled(state)",
    )
