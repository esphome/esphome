import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.audio_dac import AudioDac
import esphome.config_validation as cv
from esphome.const import CONF_AUDIO_DAC, CONF_BITS_PER_SAMPLE, CONF_ID
import esphome.final_validate as fv

CODEOWNERS = ["@kbx81"]
DEPENDENCIES = ["i2c"]

es8156_ns = cg.esphome_ns.namespace("es8156")
ES8156 = es8156_ns.class_("ES8156", AudioDac, cg.Component, i2c.I2CDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ES8156),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x08))
)


def _final_validate(config):
    full_config = fv.full_config.get()

    # Check all speaker configurations for ones that reference this es8156
    speaker_configs = full_config.get("speaker", [])
    for speaker_config in speaker_configs:
        audio_dac_id = speaker_config.get(CONF_AUDIO_DAC)
        if (
            audio_dac_id is not None
            and audio_dac_id == config[CONF_ID]
            and (bits_per_sample := speaker_config.get(CONF_BITS_PER_SAMPLE))
            is not None
            and bits_per_sample > 24
        ):
            raise cv.Invalid(
                f"ES8156 does not support more than 24 bits per sample. "
                f"The speaker referencing this audio_dac has bits_per_sample set to {bits_per_sample}."
            )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
