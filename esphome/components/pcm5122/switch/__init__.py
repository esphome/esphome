import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import CONF_POWER_MODE, ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from ..audio_dac import CONF_PCM5122, PCM5122

CONFIG_SCHEMA = switch.switch_schema(
    switch.ApplySwitch,
    entity_category=ENTITY_CATEGORY_CONFIG,
).extend(
    {
        cv.GenerateID(CONF_PCM5122): cv.use_id(PCM5122),
        cv.Optional(CONF_POWER_MODE, default="powerdown"): cv.one_of(
            "standby", "powerdown", lower=True
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_PCM5122])
    setter = "set_standby" if config[CONF_POWER_MODE] == "standby" else "set_powerdown"
    # The switch shows the new state only once the DAC accepted it
    await switch.sub_switches(config, parent=hub).create(
        config, None, apply=f"{setter}(state)", publish="on_success"
    )
