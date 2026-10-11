import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_CHIP  # noqa: F401
from esphome.types import ConfigType

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


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ES8388_ID])
    selects = select.sub_selects(config, parent=hub)
    await selects(
        CONF_DAC_OUTPUT, hub.set_dac_output_select, options=["LINE1", "LINE2", "BOTH"]
    )
    await selects(
        CONF_ADC_INPUT_MIC,
        hub.set_adc_input_mic_select,
        options=["LINE1", "LINE2", "DIFFERENCE"],
    )
