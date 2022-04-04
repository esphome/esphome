import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch, hbridge
from esphome.const import (
    CONF_ID,
    CONF_RESTORE_MODE,
    CONF_DURATION,
)

CODEOWNERS = ["@FaBjE"]
AUTO_LOAD = ["hbridge"]

HBridgeSwitch = hbridge.hbridge_ns.class_(
    "HBridgeSwitch", switch.Switch, hbridge.HBridge
)

HBridgeSwitchRestoreMode = hbridge.hbridge_ns.enum("HBridgeSwitchRestoreMode")

RESTORE_MODES = {
    "RESTORE_DEFAULT_OFF": HBridgeSwitchRestoreMode.HBRIDGE_SWITCH_RESTORE_DEFAULT_OFF,
    "RESTORE_DEFAULT_ON": HBridgeSwitchRestoreMode.HBRIDGE_SWITCH_RESTORE_DEFAULT_ON,
    "ALWAYS_OFF": HBridgeSwitchRestoreMode.HBRIDGE_SWITCH_ALWAYS_OFF,
    "ALWAYS_ON": HBridgeSwitchRestoreMode.HBRIDGE_SWITCH_ALWAYS_ON,
    "RESTORE_INVERTED_DEFAULT_OFF": HBridgeSwitchRestoreMode.HBRIDGE_SWITCH_RESTORE_INVERTED_DEFAULT_OFF,
    "RESTORE_INVERTED_DEFAULT_ON": HBridgeSwitchRestoreMode.HBRIDGE_SWITCH_RESTORE_INVERTED_DEFAULT_ON,
}

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(HBridgeSwitch),
        cv.Optional(CONF_RESTORE_MODE, default="RESTORE_DEFAULT_OFF"): cv.enum(
            RESTORE_MODES, upper=True, space="_"
        ),
        cv.Optional(CONF_DURATION, default="0ms"): cv.positive_time_period_milliseconds,
    }
).extend(hbridge.HBRIDGE_CONFIG_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    cg.add(var.set_switching_duration(config[CONF_DURATION]))

    # HBridge driver config
    await hbridge.hbridge_setup(config, var)
