import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_HIGH,
    CONF_LOW,
    CONF_DURATION,
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from .. import (
    CONF_KT0803_ID,
    KT0803Component,
    kt0803_ns,
    CONF_REF_CLK,
    CONF_XTAL,
    CONF_ALC,
    CONF_SILENCE,
    CONF_DEVIATION,
    CONF_PRE_EMPHASIS,
    CONF_PILOT_TONE_AMPLITUDE,
    CONF_BASS_BOOST_CONTROL,
    CONF_AUDIO_LIMITER_LEVEL,
    CONF_SWITCH_MODE,
    CONF_SEL,
    CONF_HOLD_TIME,
    CONF_ATTACK_TIME,
    CONF_DECAY_TIME,
    CONF_HIGH_COUNTER,
    CONF_LOW_COUNTER,
    ICON_SLEEP,
    ICON_SINE_WAVE,
    ICON_SPEAKER,
    FREQUENCY_DEVIATION,
    PRE_EMPHASIS,
    PILOT_TONE_AMPLITUDE,
    BASS_BOOST_CONTROL,
    AUDIO_LIMITER_LEVEL,
    SWITCH_MODE,
    REFERENCE_CLOCK,
    XTAL_SEL,
    ALC_TIME,
    ALC_HOLD_TIME,
    ALC_HIGH,
    ALC_LOW,
    SILENCE_HIGH,
    SILENCE_LOW,
    SILENCE_LOW_AND_HIGH_LEVEL_DURATION_TIME,
    SILENCE_HIGH_LEVEL_COUNTER,
    SILENCE_LOW_LEVEL_COUNTER,
    for_each_conf,
)

FrequencyDeviationSelect = kt0803_ns.class_("FrequencyDeviationSelect", select.Select)
PreEmphasisSelect = kt0803_ns.class_("PreEmphasisSelect", select.Select)
PilotToneAmplitudeSelect = kt0803_ns.class_("PilotToneAmplitudeSelect", select.Select)
BassBoostControlSelect = kt0803_ns.class_("BassBoostControlSelect", select.Select)
AudioLimiterLevelSelect = kt0803_ns.class_("AudioLimiterLevelSelect", select.Select)
SwitchModeSelect = kt0803_ns.class_("SwitchModeSelect", select.Select)
RefClkSelect = kt0803_ns.class_("RefClkSelect", select.Select)
XtalSelSelect = kt0803_ns.class_("XtalSelSelect", select.Select)
AlcAttackTimeSelect = kt0803_ns.class_("AlcAttackTimeSelect", select.Select)
AlcDecayTimeSelect = kt0803_ns.class_("AlcDecayTimeSelect", select.Select)
AlcHoldTimeSelect = kt0803_ns.class_("AlcHoldTimeSelect", select.Select)
AlcHighSelect = kt0803_ns.class_("AlcHighSelect", select.Select)
AlcLowSelect = kt0803_ns.class_("AlcLowSelect", select.Select)
SilenceDurationSelect = kt0803_ns.class_("SilenceDurationSelect", select.Select)
SilenceHighSelect = kt0803_ns.class_("SilenceHighSelect", select.Select)
SilenceLowSelect = kt0803_ns.class_("SilenceLowSelect", select.Select)
SilenceHighCounterSelect = kt0803_ns.class_("SilenceHighCounterSelect", select.Select)
SilenceLowCounterSelect = kt0803_ns.class_("SilenceLowCounterSelect", select.Select)

REF_CLK_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SEL): select.select_schema(
            RefClkSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
    }
)

XTAL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SEL): select.select_schema(
            XtalSelSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_PULSE,
        ),
    }
)

ALC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ATTACK_TIME): select.select_schema(
            AlcAttackTimeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_DECAY_TIME): select.select_schema(
            AlcDecayTimeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_HOLD_TIME): select.select_schema(
            AlcHoldTimeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_HIGH): select.select_schema(
            AlcHighSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_LOW): select.select_schema(
            AlcLowSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
    }
)

SILENCE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DURATION): select.select_schema(
            SilenceDurationSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_HIGH): select.select_schema(
            SilenceHighSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_LOW): select.select_schema(
            SilenceLowSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_HIGH_COUNTER): select.select_schema(
            SilenceHighCounterSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_LOW_COUNTER): select.select_schema(
            SilenceLowCounterSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_KT0803_ID): cv.use_id(KT0803Component),
        cv.Optional(CONF_DEVIATION): select.select_schema(
            FrequencyDeviationSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_PRE_EMPHASIS): select.select_schema(
            PreEmphasisSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_PILOT_TONE_AMPLITUDE): select.select_schema(
            PilotToneAmplitudeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_BASS_BOOST_CONTROL): select.select_schema(
            BassBoostControlSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SPEAKER,
        ),
        cv.Optional(CONF_AUDIO_LIMITER_LEVEL): select.select_schema(
            AudioLimiterLevelSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_SWITCH_MODE): select.select_schema(
            SwitchModeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            # icon=ICON_,
        ),
        cv.Optional(CONF_REF_CLK): REF_CLK_SCHEMA,
        cv.Optional(CONF_XTAL): XTAL_SCHEMA,
        cv.Optional(CONF_ALC): ALC_SCHEMA,
        cv.Optional(CONF_SILENCE): SILENCE_SCHEMA,
    }
)

VARIABLES = {
    None: [
        [CONF_DEVIATION, FREQUENCY_DEVIATION],
        [CONF_PRE_EMPHASIS, PRE_EMPHASIS],
        [CONF_PILOT_TONE_AMPLITUDE, PILOT_TONE_AMPLITUDE],
        [CONF_BASS_BOOST_CONTROL, BASS_BOOST_CONTROL],
        [CONF_AUDIO_LIMITER_LEVEL, AUDIO_LIMITER_LEVEL],
        [CONF_SWITCH_MODE, SWITCH_MODE],
    ],
    CONF_REF_CLK: [
        [CONF_SEL, REFERENCE_CLOCK],
    ],
    CONF_XTAL: [
        [CONF_SEL, XTAL_SEL],
    ],
    CONF_ALC: [
        [CONF_ATTACK_TIME, ALC_TIME],
        [CONF_DECAY_TIME, ALC_TIME],
        [CONF_HOLD_TIME, ALC_HOLD_TIME],
        [CONF_HIGH, ALC_HIGH],
        [CONF_LOW, ALC_LOW],
    ],
    CONF_SILENCE: [
        [CONF_DURATION, SILENCE_LOW_AND_HIGH_LEVEL_DURATION_TIME],
        [CONF_HIGH, SILENCE_HIGH],
        [CONF_LOW, SILENCE_LOW],
        [CONF_HIGH_COUNTER, SILENCE_HIGH_LEVEL_COUNTER],
        [CONF_LOW_COUNTER, SILENCE_LOW_LEVEL_COUNTER],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_KT0803_ID])

    async def new_select(c, args, setter):
        s = await select.new_select(c, options=list(args[1].keys()))
        await cg.register_parented(s, parent)
        cg.add(getattr(parent, setter + "_select")(s))

    await for_each_conf(config, VARIABLES, new_select)
