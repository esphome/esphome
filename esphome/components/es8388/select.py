import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_INITIAL_OPTION,
    ENTITY_CATEGORY_CONFIG,
    ICON_CHIP,  # noqa: F401
)
from esphome.types import ConfigType

from .audio_dac import CONF_ES8388_ID, ES8388, es8388_ns

CONF_DAC_OUTPUT = "dac_output"
CONF_ADC_INPUT_MIC = "adc_input_mic"

DacOutputLine = es8388_ns.enum("DacOutputLine")
DAC_OUTPUT_OPTIONS = DacOutputLine.options(
    "LINE1", "LINE2", "BOTH", prefix="DAC_OUTPUT_"
)
DAC_OUTPUT_OPTION_NAMES = list(DAC_OUTPUT_OPTIONS)
AdcInputMicLine = es8388_ns.enum("AdcInputMicLine")
ADC_INPUT_MIC_OPTIONS = AdcInputMicLine.options(
    "LINE1", "LINE2", "DIFFERENCE", prefix="ADC_INPUT_MIC_"
)
ADC_INPUT_MIC_OPTION_NAMES = list(ADC_INPUT_MIC_OPTIONS)


CONFIG_SCHEMA = cv.All(
    {
        cv.GenerateID(CONF_ES8388_ID): cv.use_id(ES8388),
        cv.Optional(CONF_DAC_OUTPUT): select.select_schema(
            select.ApplySelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_CHIP,
        ).extend(
            {
                cv.Optional(CONF_INITIAL_OPTION): cv.enum(
                    DAC_OUTPUT_OPTIONS, upper=True
                ),
            }
        ),
        cv.Optional(CONF_ADC_INPUT_MIC): select.select_schema(
            select.ApplySelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_CHIP,
        ).extend(
            {
                cv.Optional(CONF_INITIAL_OPTION): cv.enum(
                    ADC_INPUT_MIC_OPTIONS, upper=True
                ),
            }
        ),
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_ES8388_SELECT")
    hub = await cg.get_variable(config[CONF_ES8388_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_DAC_OUTPUT,
        hub.set_dac_output_select,
        options=DAC_OUTPUT_OPTION_NAMES,
        apply="set_dac_output(static_cast<es8388::DacOutputLine>(index))",
        publish="before",
    )
    await selects(
        CONF_ADC_INPUT_MIC,
        hub.set_adc_input_mic_select,
        options=ADC_INPUT_MIC_OPTION_NAMES,
        apply="set_adc_input_mic(static_cast<es8388::AdcInputMicLine>(index))",
        publish="before",
    )
    if (
        initial := config.get(CONF_DAC_OUTPUT, {}).get(CONF_INITIAL_OPTION)
    ) is not None:
        cg.add(hub.set_dac_output_initial(initial))
    if (
        initial := config.get(CONF_ADC_INPUT_MIC, {}).get(CONF_INITIAL_OPTION)
    ) is not None:
        cg.add(hub.set_adc_input_mic_initial(initial))
