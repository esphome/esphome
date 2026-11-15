import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG, ICON_CHIP
from esphome.types import ConfigType

from .audio_adc import CONF_PCM1863_ID, PCM1863, PCM1863_INPUT_ENUM

# The select reports the position of the chosen option rather than its value, so this
# order has to stay in step with INPUT_BY_INDEX in pcm1863.cpp
PCM1863_INPUT_OPTIONS = list(PCM1863_INPUT_ENUM)

CONFIG_SCHEMA = select.select_schema(
    select.ApplySelect,
    entity_category=ENTITY_CATEGORY_CONFIG,
    icon=ICON_CHIP,
).extend(
    {
        cv.GenerateID(CONF_PCM1863_ID): cv.use_id(PCM1863),
    }
)


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_PCM1863_SELECT")
    hub = await cg.get_variable(config[CONF_PCM1863_ID])
    # The ADC publishes the new option only once the I2C write went through
    await select.sub_selects(config, parent=hub).create(
        config,
        hub.set_input_select,
        options=PCM1863_INPUT_OPTIONS,
        apply="set_input_index(index)",
        publish="on_success",
    )
