import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import CONF_TOPTRONIC_ID, TopTronicComponent, toptronic

CONF_DEBUG_MODE = "debug_mode"

# Generated as a C++ `enum class DebugMode`, so the YAML keys and the C++
# enumerators are checked against each other at code-generation time.
DebugMode = toptronic.enum("DebugMode", is_class=True)

DEBUG_MODES = {
    "CANDUMP": DebugMode.CANDUMP,  # log every CAN frame (tag "candump")
    "FIND_CAN_ID": DebugMode.FIND_CAN_ID,  # log only 0x42/0x40 frames at WARN
}

# A debug-mode switch controls exactly one build-wide debug flag. write_state()
# forwards to the hub, and a callback from the hub keeps every switch instance in
# sync with the actual logging state, so the YAML needs no cross-turn-off
# automation.
TopTronicDebugSwitch = toptronic.class_(
    "TopTronicDebugSwitch", switch.Switch, cg.Component
)

CONFIG_SCHEMA = (
    switch.switch_schema(
        TopTronicDebugSwitch,
        default_restore_mode="ALWAYS_OFF",
    )
    .extend(
        {
            cv.GenerateID(CONF_TOPTRONIC_ID): cv.use_id(TopTronicComponent),
            cv.Required(CONF_DEBUG_MODE): cv.enum(DEBUG_MODES, upper=True),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    tt = await cg.get_variable(config[CONF_TOPTRONIC_ID])
    var = cg.new_Pvariable(config[CONF_ID])
    await switch.register_switch(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_parent(tt))
    cg.add(var.set_debug_mode(config[CONF_DEBUG_MODE]))
