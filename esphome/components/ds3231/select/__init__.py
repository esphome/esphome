import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import ENTITY_CATEGORY_CONFIG
from esphome.types import ConfigType

from .. import CONF_DS3231_ID, DS3231Component, ds3231_ns

DEPENDENCIES = ["ds3231"]

CONF_OUTPUT_MODE = "output_mode"
CONF_SQUARE_WAVE_FREQUENCY = "square_wave_frequency"

# Order matters: the index is what the C++ control() / publish_state() use.
OUTPUT_MODE_OPTIONS = ["alarm interrupt", "square wave"]
SQUARE_WAVE_FREQUENCY_OPTIONS = ["1 Hz", "1.024 kHz", "4.096 kHz", "8.192 kHz"]

DS3231OutputModeSelect = ds3231_ns.class_("DS3231OutputModeSelect", select.Select)
DS3231SquareWaveFrequencySelect = ds3231_ns.class_(
    "DS3231SquareWaveFrequencySelect", select.Select
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_DS3231_ID): cv.use_id(DS3231Component),
        cv.Optional(CONF_OUTPUT_MODE): select.select_schema(
            DS3231OutputModeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:sine-wave",
        ),
        cv.Optional(CONF_SQUARE_WAVE_FREQUENCY): select.select_schema(
            DS3231SquareWaveFrequencySelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon="mdi:sine-wave",
        ),
    }
).add_extra(cv.has_at_least_one_key(CONF_OUTPUT_MODE, CONF_SQUARE_WAVE_FREQUENCY))


async def to_code(config: ConfigType) -> None:
    parent = await cg.get_variable(config[CONF_DS3231_ID])

    for key, options, setter in (
        (CONF_OUTPUT_MODE, OUTPUT_MODE_OPTIONS, "set_output_mode_select"),
        (
            CONF_SQUARE_WAVE_FREQUENCY,
            SQUARE_WAVE_FREQUENCY_OPTIONS,
            "set_square_wave_frequency_select",
        ),
    ):
        if (conf := config.get(key)) is not None:
            sel = await select.new_select(conf, options=options)
            await cg.register_parented(sel, config[CONF_DS3231_ID])
            cg.add(getattr(parent, setter)(sel))
