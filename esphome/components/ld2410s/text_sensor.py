import esphome.codegen as cg
from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_DIAGNOSTIC, ICON_CHIP

from . import CONF_LD2410S_ID, LD2410S

# , ld2410s_ns

# LD2410STextSensor = ld2410s_ns.class_(
#     "LD2410STextSensor", text_sensor.TextSensor, cg.Component
# )

CONF_ENERGY_VALUES = "energy_values"
CONF_FW_VERSION = "fw_version"
CONF_THRESHOLD_TRIGGERS = "threshold_triggers"
CONF_THRESHOLD_HOLDS = "threshold_holds"
CONF_THRESHOLD_SNRS = "threshold_snrs"

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_LD2410S_ID): cv.use_id(LD2410S),
    cv.Optional(CONF_FW_VERSION): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC, icon=ICON_CHIP
    ),
    cv.Optional(CONF_THRESHOLD_TRIGGERS): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:tune-variant",
    ),
    cv.Optional(CONF_THRESHOLD_HOLDS): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:tune-variant",
    ),
    cv.Optional(CONF_THRESHOLD_SNRS): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:tune-variant",
    ),
    cv.Optional(CONF_ENERGY_VALUES): text_sensor.text_sensor_schema(
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        icon="mdi:lightning-bolt",
    ),
}


async def to_code(config):
    ld2410s = await cg.get_variable(config[CONF_LD2410S_ID])
    if fw_version_config := config.get(CONF_FW_VERSION):
        sens = await text_sensor.new_text_sensor(fw_version_config)
        cg.add(ld2410s.set_fw_version_text_sensor(sens))
    if threshold_trigger_config := config.get(CONF_THRESHOLD_TRIGGERS):
        sens = await text_sensor.new_text_sensor(threshold_trigger_config)
        cg.add(ld2410s.set_threshold_trigger_text_sensor(sens))
    if threshold_hold_config := config.get(CONF_THRESHOLD_HOLDS):
        sens = await text_sensor.new_text_sensor(threshold_hold_config)
        cg.add(ld2410s.set_threshold_hold_text_sensor(sens))
    if threshold_snr_config := config.get(CONF_THRESHOLD_SNRS):
        sens = await text_sensor.new_text_sensor(threshold_snr_config)
        cg.add(ld2410s.set_threshold_snr_text_sensor(sens))
    if energy_values_config := config.get(CONF_ENERGY_VALUES):
        sens = await text_sensor.new_text_sensor(energy_values_config)
        cg.add(ld2410s.set_energy_values_text_sensor(sens))
