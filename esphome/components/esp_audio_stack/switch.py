"""Switch platform for ESP Audio Stack - AEC enable/disable"""

import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG

from . import CONF_ESP_AUDIO_STACK_ID, ESPAudioStack, esp_audio_stack_ns

CONF_AEC = "aec"

# Switch class
AECSwitch = esp_audio_stack_ns.class_("AECSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESP_AUDIO_STACK_ID): cv.use_id(ESPAudioStack),
        cv.Optional(CONF_AEC): switch.switch_schema(
            AECSwitch,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:account-voice",
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ESP_AUDIO_STACK_ID])

    if CONF_AEC in config:
        conf = config[CONF_AEC]
        var = await switch.new_switch(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
