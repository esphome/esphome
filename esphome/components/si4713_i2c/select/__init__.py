import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANALOG,
    CONF_ATTENUATION,
    CONF_CHANNELS,
    CONF_MODE,
    CONF_SOURCE,
    CONF_PRESET,
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from .. import (
    CONF_SI4713_ID,
    Si4713Component,
    si4713_ns,
    CONF_DIGITAL,
    CONF_REFCLK,
    CONF_ACOMP,
    CONF_PRE_EMPHASIS,
    CONF_SAMPLE_BITS,
    CONF_CLOCK_EDGE,
    CONF_ATTACK,
    CONF_RELEASE,
    ICON_SINE_WAVE,
    ICON_RESISTOR,
    ICON_EAR_HEARING,
    PRE_EMPHASIS,
    LINE_ATTENUATION,
    SAMPLE_BITS,
    SAMPLE_CHANNELS,
    DIGITAL_MODE,
    DIGITAL_CLOCK_EDGE,
    REFCLK_SOURCE,
    ACOMP_ATTACK,
    ACOMP_RELEASE,
    ACOMP_PRESET,
    for_each_conf,
)

PreEmphasisSelect = si4713_ns.class_("PreEmphasisSelect", select.Select)
AnalogAttenuationSelect = si4713_ns.class_("AnalogAttenuationSelect", select.Select)
DigitalSampleBitsSelect = si4713_ns.class_("DigitalSampleBitsSelect", select.Select)
DigitalChannelsSelect = si4713_ns.class_("DigitalChannelsSelect", select.Select)
DigitalModeSelect = si4713_ns.class_("DigitalModeSelect", select.Select)
DigitalClockEdgeSelect = si4713_ns.class_("DigitalClockEdgeSelect", select.Select)
RefClkSourceSelect = si4713_ns.class_("RefClkSourceSelect", select.Select)
AcompAttackSelect = si4713_ns.class_("AcompAttackSelect", select.Select)
AcompReleaseSelect = si4713_ns.class_("AcompReleaseSelect", select.Select)
AcompPresetSelect = si4713_ns.class_("AcompPresetSelect", select.Select)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SI4713_ID): cv.use_id(Si4713Component),
        cv.Optional(CONF_PRE_EMPHASIS): select.select_schema(
            PreEmphasisSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_ANALOG): cv.Schema(
            {
                cv.Optional(CONF_ATTENUATION): select.select_schema(
                    AnalogAttenuationSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_RESISTOR,
                ),
            }
        ),
        cv.Optional(CONF_DIGITAL): cv.Schema(
            {
                cv.Optional(CONF_SAMPLE_BITS): select.select_schema(
                    DigitalSampleBitsSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    # icon=ICON_,
                ),
                cv.Optional(CONF_CHANNELS): select.select_schema(
                    DigitalChannelsSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_EAR_HEARING,
                ),
                cv.Optional(CONF_MODE): select.select_schema(
                    DigitalModeSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    # icon=ICON_,
                ),
                cv.Optional(CONF_CLOCK_EDGE): select.select_schema(
                    DigitalClockEdgeSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_PULSE,
                ),
            }
        ),
        cv.Optional(CONF_REFCLK): cv.Schema(
            {
                cv.Optional(CONF_SOURCE): select.select_schema(
                    RefClkSourceSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_PULSE,
                ),
            }
        ),
        cv.Optional(CONF_ACOMP): cv.Schema(
            {
                cv.Optional(CONF_ATTACK): select.select_schema(
                    AcompAttackSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_SINE_WAVE,
                ),
                cv.Optional(CONF_RELEASE): select.select_schema(
                    AcompReleaseSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_SINE_WAVE,
                ),
                cv.Optional(CONF_PRESET): select.select_schema(
                    AcompPresetSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_SINE_WAVE,
                ),
            }
        ),
    }
)

VARIABLES = {
    None: [
        [CONF_PRE_EMPHASIS, PRE_EMPHASIS],
    ],
    CONF_ANALOG: [
        [CONF_ATTENUATION, LINE_ATTENUATION],
    ],
    CONF_DIGITAL: [
        [CONF_SAMPLE_BITS, SAMPLE_BITS],
        [CONF_CHANNELS, SAMPLE_CHANNELS],
        [CONF_MODE, DIGITAL_MODE],
        [CONF_CLOCK_EDGE, DIGITAL_CLOCK_EDGE],
    ],
    CONF_REFCLK: [
        [CONF_SOURCE, REFCLK_SOURCE],
    ],
    CONF_ACOMP: [
        [CONF_ATTACK, ACOMP_ATTACK],
        [CONF_RELEASE, ACOMP_RELEASE],
        [CONF_PRESET, ACOMP_PRESET],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SI4713_ID])

    async def new_select(c, args, setter):
        s = await select.new_select(c, options=list(args[1].keys()))
        await cg.register_parented(s, parent)
        cg.add(getattr(parent, setter + "_select")(s))

    await for_each_conf(config, VARIABLES, new_select)
