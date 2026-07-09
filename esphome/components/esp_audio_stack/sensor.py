"""Sensor platform for ESP Audio Stack diagnostics."""

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC

from . import CONF_ESP_AUDIO_STACK_ID, ESPAudioStack, esp_audio_stack_ns

CONF_SLOT = "slot"
CONF_TDM_SLOT_LEVELS = "tdm_slot_levels"

TdmSlotLevelSensor = esp_audio_stack_ns.class_(
    "TdmSlotLevelSensor",
    sensor.Sensor,
    cg.PollingComponent,
    cg.Parented.template(ESPAudioStack),
)


def _slot_level_schema():
    return (
        sensor.sensor_schema(
            TdmSlotLevelSensor,
            unit_of_measurement="dBFS",
            accuracy_decimals=1,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon="mdi:microphone-message",
        )
        .extend(cv.polling_component_schema("500ms"))
        .extend({cv.Required(CONF_SLOT): cv.int_range(min=0, max=7)})
    )


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ESP_AUDIO_STACK_ID): cv.use_id(ESPAudioStack),
        cv.Optional(CONF_TDM_SLOT_LEVELS): cv.ensure_list(_slot_level_schema()),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ESP_AUDIO_STACK_ID])

    for conf in config.get(CONF_TDM_SLOT_LEVELS, []):
        slot = conf[CONF_SLOT]
        cg.add(parent.set_tdm_slot_level_sensor_enabled(slot, True))
        var = await sensor.new_sensor(conf)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_slot(slot))
