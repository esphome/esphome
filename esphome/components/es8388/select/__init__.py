import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_CHIP  # noqa: F401

from ..audio_dac import CONF_ES8388_ID, ES8388, es8388_ns

CONF_DAC_OUTPUT = "dac_output"
CONF_ADC_INPUT_MIC = "adc_input_mic"

DacOutputSelect = es8388_ns.class_("DacOutputSelect", select.Select)
ADCInputMicSelect = es8388_ns.class_("ADCInputMicSelect", select.Select)

CONFIG_SCHEMA = cv.All(
    {
        cv.GenerateID(CONF_ES8388_ID): cv.use_id(ES8388),
        cv.Optional(CONF_DAC_OUTPUT): select.select_schema(
            DacOutputSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_CHIP,
        ),
        cv.Optional(CONF_ADC_INPUT_MIC): select.select_schema(
            ADCInputMicSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_CHIP,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ES8388_ID])
    if dac_output_config := config.get(CONF_DAC_OUTPUT):
        s = await select.new_select(
            dac_output_config,
            options=["LINE1", "LINE2", "BOTH"],
        )
        await cg.register_parented(s, parent)
        cg.add(parent.set_dac_output_select(s))

    if adc_input_mic_config := config.get(CONF_ADC_INPUT_MIC):
        s = await select.new_select(
            adc_input_mic_config,
            options=["LINE1", "LINE2", "DIFFERENCE"],
        )
        await cg.register_parented(s, parent)
        cg.add(parent.set_adc_input_mic_select(s))
