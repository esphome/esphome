import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_POWER_MODE, ENTITY_CATEGORY_CONFIG

from ..audio_dac import CONF_PCM5122, PCM5122, pcm5122_ns

PCM5122PowerSwitch = pcm5122_ns.class_("PCM5122PowerSwitch", switch.Switch)

pcm5122_power_switch_mode = pcm5122_ns.enum("PCM5122PowerSwitchMode")
PCM5122_POWER_SWITCH_MODE_ENUM = {
    "standby": pcm5122_power_switch_mode.PCM5122_POWER_SWITCH_MODE_STANDBY,
    "powerdown": pcm5122_power_switch_mode.PCM5122_POWER_SWITCH_MODE_POWERDOWN,
}

CONFIG_SCHEMA = switch.switch_schema(
    PCM5122PowerSwitch,
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(
    {
        cv.GenerateID(CONF_PCM5122): cv.use_id(PCM5122),
        cv.Optional(CONF_POWER_MODE, default="powerdown"): cv.enum(
            PCM5122_POWER_SWITCH_MODE_ENUM, lower=True
        ),
    }
)


async def to_code(config):
    var = await switch.new_switch(config)
    await cg.register_parented(var, config[CONF_PCM5122])
    cg.add(var.set_power_mode(config[CONF_POWER_MODE]))
