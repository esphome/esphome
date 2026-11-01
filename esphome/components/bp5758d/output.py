from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_CURRENT, CONF_ID
from esphome.types import ConfigType

from . import BP5758D

DEPENDENCIES = ["bp5758d"]

CONF_BP5758D_ID = "bp5758d_id"
CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_BP5758D_ID): cv.use_id(BP5758D),
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.Required(CONF_CHANNEL): cv.int_range(min=1, max=5),
        cv.Optional(CONF_CURRENT, default=10): cv.int_range(min=0, max=90),
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="bp5758d", hub_key=CONF_BP5758D_ID
)


async def to_code(config: ConfigType) -> None:
    # Channels are OUT1-OUT5 in YAML; the hub is 0-based. The current is baked into the call,
    # so channels with the same current share one function.
    await output.sub_float_outputs(config, parent=config[CONF_BP5758D_ID]).create(
        config,
        None,
        config[CONF_CHANNEL] - 1,
        apply=f"write_channel(index, state, {config[CONF_CURRENT]})",
    )
