"""Number platform for ESP Audio Stack - mic gain and Master Volume"""

import esphome.codegen as cg
from esphome.components import number, speaker
import esphome.config_validation as cv
from esphome.const import (
    CONF_MAX_VALUE,
    CONF_MIC_GAIN,
    CONF_MIN_VALUE,
    ENTITY_CATEGORY_CONFIG,
    UNIT_PERCENT,
)

from . import CONF_ESP_AUDIO_STACK_ID, ESPAudioStack, esp_audio_stack_ns

CONF_MASTER_VOLUME = "master_volume"
CONF_SPEAKER_ID = "speaker_id"

# Number classes
MicGainNumber = esp_audio_stack_ns.class_("MicGainNumber", number.Number, cg.Component)
MasterVolumeNumber = esp_audio_stack_ns.class_(
    "MasterVolumeNumber", number.Number, cg.Component
)

MIC_GAIN_SCHEMA = number.number_schema(
    MicGainNumber,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon="mdi:microphone",
).extend(
    {
        cv.Optional(CONF_MIN_VALUE, default=-20.0): cv.float_range(min=-20.0, max=30.0),
        cv.Optional(CONF_MAX_VALUE, default=30.0): cv.float_range(min=-20.0, max=30.0),
    }
)


def _validate_mic_gain_range(config):
    if config[CONF_MIN_VALUE] >= config[CONF_MAX_VALUE]:
        raise cv.Invalid("mic_gain min_value must be lower than max_value")
    return config


MIC_GAIN_SCHEMA = cv.All(MIC_GAIN_SCHEMA, _validate_mic_gain_range)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESP_AUDIO_STACK_ID): cv.use_id(ESPAudioStack),
        cv.Optional(CONF_MIC_GAIN): MIC_GAIN_SCHEMA,
        cv.Optional(CONF_MASTER_VOLUME): number.number_schema(
            MasterVolumeNumber,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:volume-high",
            unit_of_measurement=UNIT_PERCENT,
        ).extend(
            {
                cv.Optional(CONF_SPEAKER_ID): cv.use_id(speaker.Speaker),
            }
        ),
    }
)


async def _setup_mic_gain(config, key, parent):
    if key in config:
        conf = config[key]
        var = await number.new_number(
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=1.0,
        )
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_min_db(conf[CONF_MIN_VALUE]))
        cg.add(var.set_max_db(conf[CONF_MAX_VALUE]))


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ESP_AUDIO_STACK_ID])

    await _setup_mic_gain(config, CONF_MIC_GAIN, parent)

    if CONF_MASTER_VOLUME in config:
        conf = config[CONF_MASTER_VOLUME]
        var = await number.new_number(
            conf,
            min_value=0.0,
            max_value=100.0,
            step=5.0,
        )
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
        if CONF_SPEAKER_ID in conf:
            speaker_var = await cg.get_variable(conf[CONF_SPEAKER_ID])
            cg.add(var.set_speaker(speaker_var))
