import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import output, switch
from esphome.const import CONF_ID, CONF_OUTPUT, CONF_RESTORE_MODE
from .. import output_ns

OutputSwitch = output_ns.class_("OutputSwitch", switch.Switch, cg.Component)

OutputSwitchRestoreMode = output_ns.enum("OutputSwitchRestoreMode")
RESTORE_MODES = {
    "RESTORE_DEFAULT_OFF": OutputSwitchRestoreMode.OUTPUT_SWITCH_RESTORE_DEFAULT_OFF,
    "RESTORE_DEFAULT_ON": OutputSwitchRestoreMode.OUTPUT_SWITCH_RESTORE_DEFAULT_ON,
    "ALWAYS_OFF": OutputSwitchRestoreMode.OUTPUT_SWITCH_ALWAYS_OFF,
    "ALWAYS_ON": OutputSwitchRestoreMode.OUTPUT_SWITCH_ALWAYS_ON,
    "RESTORE_INVERTED_DEFAULT_OFF": OutputSwitchRestoreMode.OUTPUT_SWITCH_RESTORE_INVERTED_DEFAULT_OFF,
    "RESTORE_INVERTED_DEFAULT_ON": OutputSwitchRestoreMode.OUTPUT_SWITCH_RESTORE_INVERTED_DEFAULT_ON,
}

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(OutputSwitch),
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        cv.Optional(CONF_RESTORE_MODE, default="RESTORE_DEFAULT_OFF"): cv.enum(
            RESTORE_MODES, upper=True, space="_"
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await switch.register_switch(var, config)

    output_ = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(output_))

    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))
