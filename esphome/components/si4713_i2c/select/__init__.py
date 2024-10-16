import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from .. import (
    CONF_SI4713_ID,
    Si4713Component,
    si4713_ns,
    CONF_SECTION_ANALOG,
    CONF_SECTION_DIGITAL,
    CONF_SECTION_REFCLK,
    CONF_SECTION_COMPRESSOR,
    CONF_PRE_EMPHASIS,
    CONF_ATTENUATION,
    CONF_SAMPLE_BITS,
    CONF_CHANNELS,
    CONF_MODE,
    CONF_CLOCK_EDGE,
    CONF_SOURCE,
    CONF_ATTACK,
    CONF_RELEASE,
    CONF_PRESET,
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
        cv.Optional(CONF_SECTION_ANALOG): cv.Schema(
             {
                cv.Optional(CONF_ATTENUATION): select.select_schema(
                    AnalogAttenuationSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_RESISTOR,
                ),
             }
        ),
        cv.Optional(CONF_SECTION_DIGITAL): cv.Schema(
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
        cv.Optional(CONF_SECTION_REFCLK): cv.Schema(
             {
                cv.Optional(CONF_SOURCE): select.select_schema(
                    RefClkSourceSelect,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_PULSE,
                ),
             }
        ),
        cv.Optional(CONF_SECTION_COMPRESSOR): cv.Schema(
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


async def new_select(parent, config, id, setter, options):
    if c := config.get(id):
        s = await select.new_select(c, options=list(options.keys()))
        await cg.register_parented(s, parent)
        cg.add(setter(s))
        return s


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SI4713_ID])
    await new_select(parent, config, CONF_PRE_EMPHASIS, parent.set_pre_emphasis_select, PRE_EMPHASIS)
    if analog_config := config.get(CONF_SECTION_ANALOG):
        await new_select(parent, analog_config, CONF_ATTENUATION, parent.set_analog_attenuation_select, LINE_ATTENUATION)
    if digital_config := config.get(CONF_SECTION_DIGITAL):
        await new_select(parent, digital_config, CONF_SAMPLE_BITS, parent.set_digital_sample_bits_select, SAMPLE_BITS)
        await new_select(parent, digital_config, CONF_CHANNELS, parent.set_digital_channels_select, SAMPLE_CHANNELS)
        await new_select(parent, digital_config, CONF_MODE, parent.set_digital_mode_select, DIGITAL_MODE)
        await new_select(parent, digital_config, CONF_CLOCK_EDGE, parent.set_digital_clock_edge_select, DIGITAL_CLOCK_EDGE)
    if refclk_config := config.get(CONF_SECTION_REFCLK):
        await new_select(parent, refclk_config, CONF_SOURCE, parent.set_refclk_source_select, REFCLK_SOURCE)
    if compressor_config := config.get(CONF_SECTION_COMPRESSOR):
        await new_select(parent, compressor_config, CONF_ATTACK, parent.set_acomp_attack_select, ACOMP_ATTACK)
        await new_select(parent, compressor_config, CONF_RELEASE, parent.set_acomp_release_select, ACOMP_RELEASE)
        await new_select(parent, compressor_config, CONF_PRESET, parent.set_acomp_preset_select, ACOMP_PRESET)
