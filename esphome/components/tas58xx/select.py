import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import DOMAIN
from .audio_dac import (
    CONF_DAC_MODE,
    CONF_MIXER_MODE,
    CONF_TAS58XX_ID,
    MIXER_MODES,
    TAS58xx,
)

ICON_SPEAKER_MULTIPLE = "mdi:speaker-multiple"

# Option text of each mixer mode, in MixerMode order
MIXER_MODE_OPTIONS = {
    "stereo": "Stereo",
    "stereo_inverse": "Stereo inverse",
    "mono": "Mono",
    "left": "Left",
    "right": "Right",
}
# PBTL drives single speaker, so stereo modes are not offered
PBTL_MIXER_MODES = ("mono", "left", "right")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TAS58XX_ID): cv.use_id(TAS58xx),
        cv.Optional(CONF_MIXER_MODE): select.select_schema(
            select.ApplySelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SPEAKER_MULTIPLE,
        ),
    }
)


# audio_dac keeps one pointer to its select, so a second select would replace the first
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=select.DOMAIN,
    platform=DOMAIN,
    keys=(CONF_TAS58XX_ID,),
    message="Only one tas58xx select entry is allowed per audio_dac ('{tas58xx_id}')",
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_TAS58XX_ID])
    hub_conf = fv.get_declaring_config(config[CONF_TAS58XX_ID], CORE.config)
    modes = (
        PBTL_MIXER_MODES
        if hub_conf[CONF_DAC_MODE] == "pbtl"
        else tuple(MIXER_MODE_OPTIONS)
    )
    selects = select.sub_selects(config, parent=hub)
    # Options run from first mode of dac_mode to MIXER_MODE_RIGHT, so index is an offset from it
    await selects(
        CONF_MIXER_MODE,
        hub.set_mixer_mode_select,
        options=[MIXER_MODE_OPTIONS[mode] for mode in modes],
        apply=f"apply_mixer_mode(static_cast<tas58xx::MixerMode>({MIXER_MODES[modes[0]]} + index))",
    )
