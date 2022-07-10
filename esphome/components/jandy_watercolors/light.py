import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_ID, CONF_OUTPUT_ID, CONF_OUTPUT
from esphome import automation
from esphome.automation import maybe_simple_id

from . import jandy_light_ns

JandyWatercolorsLightOutput = jandy_light_ns.class_(
    "JandyWatercolorsLightOutput", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = light.LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(JandyWatercolorsLightOutput),
        cv.Required(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    out = await cg.get_variable(config[CONF_OUTPUT])
    cg.add(var.set_output(out))


JANDY_WATERCOLORS_NO_ARGS_ACTION_SCHEMA = maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(JandyWatercolorsLightOutput),
    }
)

JandyWatercolorsResetAction = jandy_light_ns.class_(
    "JandyWatercolorsResetAction", automation.Action
)


@automation.register_action(
    "jandy_watercolors.reset",
    JandyWatercolorsResetAction,
    JANDY_WATERCOLORS_NO_ARGS_ACTION_SCHEMA,
)
async def jandy_watercolors_reset_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)


JandyWatercolorsNextEffectAction = jandy_light_ns.class_(
    "JandyWatercolorsNextEffectAction", automation.Action
)


@automation.register_action(
    "jandy_watercolors.next_effect",
    JandyWatercolorsNextEffectAction,
    JANDY_WATERCOLORS_NO_ARGS_ACTION_SCHEMA,
)
async def jandy_watercolors_next_effect_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, paren)
