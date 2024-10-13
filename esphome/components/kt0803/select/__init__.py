import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from .. import (
    CONF_KT0803_ID,
    KT0803Component,
    kt0803_ns,
    CONF_CHIP_ID,
    CONF_PRE_EMPHASIS,
    CONF_PILOT_TONE_AMPLITUDE,
    CONF_BASS_BOOST_CONTROL,
    CONF_ALC_ATTACK_TIME,
    CONF_ALC_DECAY_TIME,
    CONF_AUDIO_LIMITER_LEVEL,
    CONF_SWITCH_MODE,
    CONF_SILENCE_HIGH,
    CONF_SILENCE_LOW,
    CONF_SILENCE_DURATION,
    CONF_SILENCE_HIGH_COUNTER,
    CONF_SILENCE_LOW_COUNTER,
    CONF_XTAL_SEL,
    CONF_FREQUENCY_DEVIATION,
    CONF_REF_CLK,
    CONF_ALC_HIGH,
    CONF_ALC_HOLD_TIME,
    CONF_ALC_LOW,
    ICON_SLEEP,
    ICON_SINE_WAVE,
    ICON_SPEAKER,
    ICON_EAR_HEARING,
    PRE_EMPHASIS,
    PILOT_TONE_AMPLITUDE,
    BASS_BOOST_CONTROL,
    ALC_TIME,
    AUDIO_LIMITER_LEVEL,
    SWITCH_MODE,
    SILENCE_HIGH,
    SILENCE_LOW,
    SILENCE_LOW_AND_HIGH_LEVEL_DURATION_TIME,
    SILENCE_HIGH_LEVEL_COUNTER,
    SILENCE_LOW_LEVEL_COUNTER,
    XTAL_SEL,
    FREQUENCY_DEVIATION,
    REFERENCE_CLOCK,
    ALC_HIGH,
    ALC_HOLD_TIME,
    ALC_LOW,    
)

PreEmphasisSelect = kt0803_ns.class_("PreEmphasisSelect", select.Select)
PilotToneAmplitudeSelect = kt0803_ns.class_("PilotToneAmplitudeSelect", select.Select)
BassBoostControlSelect = kt0803_ns.class_("BassBoostControlSelect", select.Select)
AlcAttackTimeSelect = kt0803_ns.class_("AlcAttackTimeSelect", select.Select)
AlcDecayTimeSelect = kt0803_ns.class_("AlcDecayTimeSelect", select.Select)
AudioLimiterLevelSelect = kt0803_ns.class_("AudioLimiterLevelSelect", select.Select)
SwitchModeSelect = kt0803_ns.class_("SwitchModeSelect", select.Select)
SilenceHighSelect = kt0803_ns.class_("SilenceHighSelect", select.Select)
SilenceLowSelect = kt0803_ns.class_("SilenceLowSelect", select.Select)
SilenceDurationSelect = kt0803_ns.class_("SilenceDurationSelect", select.Select)
SilenceHighCounterSelect = kt0803_ns.class_("SilenceHighCounterSelect", select.Select)
SilenceLowCounterSelect = kt0803_ns.class_("SilenceLowCounterSelect", select.Select)
XtalSelSelect = kt0803_ns.class_("XtalSelSelect", select.Select)
FrequencyDeviationSelect = kt0803_ns.class_("FrequencyDeviationSelect", select.Select)
RefClkSelect = kt0803_ns.class_("RefClkSelect", select.Select)
AlcHighSelect = kt0803_ns.class_("AlcHighSelect", select.Select)
AlcHoldTimeSelect = kt0803_ns.class_("AlcHoldTimeSelect", select.Select)
AlcLowSelect = kt0803_ns.class_("AlcLowSelect", select.Select)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_KT0803_ID): cv.use_id(KT0803Component),
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
        cv.Optional(CONF_ALC_ATTACK_TIME): select.select_schema(
            AlcAttackTimeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_ALC_DECAY_TIME): select.select_schema(
            AlcDecayTimeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_AUDIO_LIMITER_LEVEL): select.select_schema(
            AudioLimiterLevelSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_SWITCH_MODE): select.select_schema(
            SwitchModeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            #icon=ICON_,
        ),
        cv.Optional(CONF_SILENCE_HIGH): select.select_schema(
            SilenceHighSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_SILENCE_LOW): select.select_schema(
            SilenceLowSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_SILENCE_DURATION): select.select_schema(
            SilenceDurationSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_SILENCE_HIGH_COUNTER): select.select_schema(
            SilenceHighCounterSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_SILENCE_LOW_COUNTER): select.select_schema(
            SilenceLowCounterSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_XTAL_SEL): select.select_schema(
            XtalSelSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_PULSE,
        ),
        cv.Optional(CONF_FREQUENCY_DEVIATION): select.select_schema(
            FrequencyDeviationSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_REF_CLK): select.select_schema(
            RefClkSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_ALC_HIGH): select.select_schema(
            AlcHighSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_ALC_HOLD_TIME): select.select_schema(
            AlcHoldTimeSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_ALC_LOW): select.select_schema(
            AlcLowSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
    }
)


async def new_select(config, id, setter, options):
    if c := config.get(id):
        s = await select.new_select(c, options=list(options.keys()))
        await cg.register_parented(s, config[CONF_KT0803_ID])
        cg.add(setter(s))


async def to_code(config):
    c = await cg.get_variable(config[CONF_KT0803_ID])
    await new_select(config, CONF_PRE_EMPHASIS, c.set_pre_emphasis_select, PRE_EMPHASIS)
    await new_select(config, CONF_PILOT_TONE_AMPLITUDE, c.set_pilot_tone_amplitude_select, PILOT_TONE_AMPLITUDE)
    await new_select(config, CONF_BASS_BOOST_CONTROL, c.set_bass_boost_control_select, BASS_BOOST_CONTROL)
    await new_select(config, CONF_ALC_ATTACK_TIME, c.set_alc_attack_time_select, ALC_TIME)
    await new_select(config, CONF_ALC_DECAY_TIME, c.set_alc_decay_time_select, ALC_TIME)
    await new_select(config, CONF_AUDIO_LIMITER_LEVEL, c.set_audio_limiter_level_select, AUDIO_LIMITER_LEVEL)
    await new_select(config, CONF_SWITCH_MODE, c.set_switch_mode_select, SWITCH_MODE)
    await new_select(config, CONF_SILENCE_HIGH, c.set_silence_high_select, SILENCE_HIGH)
    await new_select(config, CONF_SILENCE_LOW, c.set_silence_low_select, SILENCE_LOW)
    await new_select(config, CONF_SILENCE_DURATION, c.set_silence_duration_select, SILENCE_LOW_AND_HIGH_LEVEL_DURATION_TIME)
    await new_select(config, CONF_SILENCE_HIGH_COUNTER, c.set_silence_high_counter_select, SILENCE_HIGH_LEVEL_COUNTER)
    await new_select(config, CONF_SILENCE_LOW_COUNTER, c.set_silence_low_counter_select, SILENCE_LOW_LEVEL_COUNTER)
    await new_select(config, CONF_XTAL_SEL, c.set_xtal_sel_select, XTAL_SEL)
    await new_select(config, CONF_FREQUENCY_DEVIATION, c.set_frequency_deviation_select, FREQUENCY_DEVIATION)
    await new_select(config, CONF_REF_CLK, c.set_ref_clk_select, REFERENCE_CLOCK)
    await new_select(config, CONF_ALC_HIGH, c.set_alc_high_select, ALC_HIGH)
    await new_select(config, CONF_ALC_HOLD_TIME, c.set_alc_hold_time_select, ALC_HOLD_TIME)
    await new_select(config, CONF_ALC_LOW, c.set_alc_low_select, ALC_LOW)
