from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MODE

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["i2c"]

aic3204_ns = cg.esphome_ns.namespace("aic3204")
AIC3204 = aic3204_ns.class_("AIC3204", AudioDac, cg.Component, i2c.I2CDevice)

SetAutoMuteAction = aic3204_ns.class_("SetAutoMuteAction", automation.Action)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AIC3204),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x18))
)


SET_AUTO_MUTE_ACTION_SCHEMA = cv.maybe_simple_value(
    {
        cv.GenerateID(): cv.use_id(AIC3204),
        cv.Required(CONF_MODE): cv.templatable(cv.int_range(max=7, min=0)),
    },
    key=CONF_MODE,
)


@automation.register_action(
    "aic3204.set_auto_mute_mode", SetAutoMuteAction, SET_AUTO_MUTE_ACTION_SCHEMA
)
async def aic3204_set_volume_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config.get(CONF_MODE), args, int)
    cg.add(var.set_auto_mute_mode(template_))

    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
