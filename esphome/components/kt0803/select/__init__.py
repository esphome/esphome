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
        cv.Optional(CONF_REF_CLK): select.select_schema(
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


async def new_select(p, config, id, setter, options):
    if c := config.get(id):
        s = await select.new_select(c, options=list(options.keys()))
        await cg.register_parented(s, p)
        cg.add(setter(s))
        return s


async def to_code(config):
    p = await cg.get_variable(config[CONF_KT0803_ID])
    await new_select(
        p, config, CONF_DEVIATION, p.set_deviation_select, FREQUENCY_DEVIATION
    )
    await new_select(
        p, config, CONF_PRE_EMPHASIS, p.set_pre_emphasis_select, PRE_EMPHASIS
    )
    await new_select(
        p,
        config,
        CONF_PILOT_TONE_AMPLITUDE,
        p.set_pilot_tone_amplitude_select,
        PILOT_TONE_AMPLITUDE,
    )
    await new_select(
        p,
        config,
        CONF_BASS_BOOST_CONTROL,
        p.set_bass_boost_control_select,
        BASS_BOOST_CONTROL,
    )
    await new_select(
        p,
        config,
        CONF_AUDIO_LIMITER_LEVEL,
        p.set_audio_limiter_level_select,
        AUDIO_LIMITER_LEVEL,
    )
    await new_select(p, config, CONF_SWITCH_MODE, p.set_switch_mode_select, SWITCH_MODE)
    if ref_clk_config := config.get(CONF_REF_CLK):
        await new_select(
            p, ref_clk_config, CONF_REF_CLK, p.set_ref_clk_select, REFERENCE_CLOCK
        )
    if xtal_config := config.get(CONF_XTAL):
        await new_select(p, xtal_config, CONF_SEL, p.set_xtal_sel_select, XTAL_SEL)
    if alc_config := config.get(CONF_ALC):
        await new_select(
            p, alc_config, CONF_ATTACK_TIME, p.set_alc_attack_time_select, ALC_TIME
        )
        await new_select(
            p, alc_config, CONF_DECAY_TIME, p.set_alc_decay_time_select, ALC_TIME
        )
        await new_select(
            p, alc_config, CONF_HOLD_TIME, p.set_alc_hold_time_select, ALC_HOLD_TIME
        )
        await new_select(p, alc_config, CONF_HIGH, p.set_alc_high_select, ALC_HIGH)
        await new_select(p, alc_config, CONF_LOW, p.set_alc_low_select, ALC_LOW)
    if silence_config := config.get(CONF_SILENCE):
        await new_select(
            p,
            silence_config,
            CONF_DURATION,
            p.set_silence_duration_select,
            SILENCE_LOW_AND_HIGH_LEVEL_DURATION_TIME,
        )
        await new_select(
            p, silence_config, CONF_HIGH, p.set_silence_high_select, SILENCE_HIGH
        )
        await new_select(
            p, silence_config, CONF_LOW, p.set_silence_low_select, SILENCE_LOW
        )
        await new_select(
            p,
            silence_config,
            CONF_HIGH_COUNTER,
            p.set_silence_high_counter_select,
            SILENCE_HIGH_LEVEL_COUNTER,
        )
        await new_select(
            p,
            silence_config,
            CONF_LOW_COUNTER,
            p.set_silence_low_counter_select,
            SILENCE_LOW_LEVEL_COUNTER,
        )
