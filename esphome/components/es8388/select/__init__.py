import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_INITIAL_OPTION,
    ENTITY_CATEGORY_CONFIG,
    ICON_CHIP,  # noqa: F401
)

from ..audio_dac import CONF_ES8388_ID, ES8388, es8388_ns

CONF_DAC_OUTPUT = "dac_output"
CONF_ADC_INPUT_MIC = "adc_input_mic"

DAC_OUTPUT_OPTIONS = ("LINE1", "LINE2", "BOTH")
ADC_INPUT_MIC_OPTIONS = ("LINE1", "LINE2", "DIFFERENCE")

DacOutputSelect = es8388_ns.class_("DacOutputSelect", select.Select)
ADCInputMicSelect = es8388_ns.class_("ADCInputMicSelect", select.Select)

CONFIG_SCHEMA = {
    cv.GenerateID(CONF_ES8388_ID): cv.use_id(ES8388),
    cv.Optional(CONF_DAC_OUTPUT): select.select_schema(
        DacOutputSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_CHIP,
    ).extend(
        {
            cv.Optional(CONF_INITIAL_OPTION): cv.one_of(
                *DAC_OUTPUT_OPTIONS, upper=True
            ),
        }
    ),
    cv.Optional(CONF_ADC_INPUT_MIC): select.select_schema(
        ADCInputMicSelect,
        entity_category=ENTITY_CATEGORY_CONFIG,
        icon=ICON_CHIP,
    ).extend(
        {
            cv.Optional(CONF_INITIAL_OPTION): cv.one_of(
                *ADC_INPUT_MIC_OPTIONS, upper=True
            ),
        }
    ),
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_ES8388_ID])
    if dac_output_config := config.get(CONF_DAC_OUTPUT):
        s = await select.new_select(
            dac_output_config,
            options=list(DAC_OUTPUT_OPTIONS),
        )
        await cg.register_parented(s, parent)
        cg.add(parent.set_dac_output_select(s))
        if (initial := dac_output_config.get(CONF_INITIAL_OPTION)) is not None:
            idx = DAC_OUTPUT_OPTIONS.index(initial)
            cg.add(parent.set_dac_output_initial_index(idx))

    if adc_input_mic_config := config.get(CONF_ADC_INPUT_MIC):
        s = await select.new_select(
            adc_input_mic_config,
            options=list(ADC_INPUT_MIC_OPTIONS),
        )
        await cg.register_parented(s, parent)
        cg.add(parent.set_adc_input_mic_select(s))
        if (initial := adc_input_mic_config.get(CONF_INITIAL_OPTION)) is not None:
            idx = ADC_INPUT_MIC_OPTIONS.index(initial)
            cg.add(parent.set_adc_input_mic_initial_index(idx))
