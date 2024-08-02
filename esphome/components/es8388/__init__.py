from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID, CONF_PRESET
from esphome.util import Registry

CONF_ES8388_ID = "es8388_id"

CONF_INIT_INSTRUCTIONS = "init_instructions"
CONF_MACROS = "macros"
CONF_INSTRUCTIONS = "instructions"

ES8388_MACROS = Registry()

es8388_ns = cg.esphome_ns.namespace("es8388")
ES8388Component = es8388_ns.class_("ES8388Component", cg.Component, i2c.I2CDevice)
Presets = es8388_ns.enum("ES8388Preset")
Macro = es8388_ns.class_("Macro")
MacroAction = es8388_ns.class_("ES8388MacroAction", automation.Action)

ES8388_PRESETS = {
    "raspiaudio_muse_luxe": Presets.RASPIAUDIO_MUSE_LUXE,
    "raspiaudio_radio": Presets.RASPIAUDIO_RADIO,
}


def validate_instruction_list():
    return cv.ensure_list(
        cv.Length(min=2, max=2),
        cv.ensure_list(int)
    )


CONFIG_SCHEMA = (
    cv.Schema({
        cv.GenerateID(): cv.declare_id(ES8388Component),
        cv.Optional(CONF_PRESET): cv.enum(ES8388_PRESETS, lower=True),
        cv.Optional(CONF_INIT_INSTRUCTIONS): validate_instruction_list(),
        cv.Optional(CONF_MACROS): cv.ensure_list({
            cv.Required(CONF_ID): cv.string,
            cv.Required(CONF_INSTRUCTIONS): validate_instruction_list(),
        }),
    })
    .extend(i2c.i2c_device_schema(0x10))
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    if CONF_PRESET in config:
        cg.add(var.set_preset(config[CONF_PRESET]))
    if CONF_INIT_INSTRUCTIONS in config:
        cg.add(var.set_init_instructions(config[CONF_INIT_INSTRUCTIONS]))
    if CONF_MACROS in config:
        for macro in config[CONF_MACROS]:
            ES8388_MACROS.register(macro[CONF_ID], Macro, {
                cv.Required(CONF_ID): cv.declare_id(Macro),
                cv.Required(CONF_INSTRUCTIONS): validate_instruction_list(),
            })
            cg.add(var.register_macro(macro[CONF_ID], macro[CONF_INSTRUCTIONS]))


@automation.register_action(
    "es8388.execute_macro",
    MacroAction,
    maybe_simple_id(
        {
            cv.GenerateID(CONF_ES8388_ID): cv.use_id(ES8388Component),
            cv.Required(CONF_ID): cv.string,
        },
    ),
)
async def execute_macro_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ES8388_ID])

    cg.add(var.set_macro_id(config[CONF_ID]))
    return var
