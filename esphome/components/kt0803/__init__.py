import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c, sensor, text_sensor, binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_EMPTY,
)

CODEOWNERS = ["@gabest11"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor", "binary_sensor", "number", "switch", "select", "text"]
MULTI_CONF = True

UNIT_MEGA_HERTZ = "MHz"
UNIT_KILO_HERTZ = "kHz"
UNIT_MILLI_VOLT = "mV"
UNIT_MICRO_AMPERE = "mA"
UNIT_DECIBEL_MICRO_VOLT = "dBµV"

ICON_VOLUME_MUTE = "mdi:volume-mute"
ICON_EAR_HEARING = "mdi:ear-hearing"
ICON_RADIO_TOWER = "mdi:radio-tower"
ICON_SLEEP = "mdi:sleep"
ICON_SINE_WAVE = "mdi:sine-wave"
ICON_RESISTOR = "mdi:resistor"
ICON_FORMAT_TEXT = "mdi:format-text"
ICON_SPEAKER = "mdi:speaker"

kt0803_ns = cg.esphome_ns.namespace("kt0803")
KT0803Component = kt0803_ns.class_(
    "KT0803Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_KT0803_ID = "kt0803_id"
CONF_CHIP_ID = "chip_id"
CONF_PW_OK = "pw_ok"
CONF_SLNCID = "slncid"
CONF_PGA = "pga"
CONF_RFGAIN = "rfgain"
CONF_MUTE = "mute"
CONF_MONO = "mono"
CONF_PRE_EMPHASIS = "pre_emphasis"
CONF_PILOT_TONE_AMPLITUDE = "pilot_tone_amplitude"
CONF_BASS_BOOST_CONTROL = "bass_boost_control"
CONF_ALC_ENABLE = "alc_enable"
CONF_AUTO_PA_DOWN = "auto_pa_down"
CONF_PA_DOWN = "pa_down"
CONF_STANDBY_ENABLE = "standby_enable"
CONF_ALC_ATTACK_TIME = "alc_attack_time"
CONF_ALC_DECAY_TIME = "alc_decay_time"
CONF_PA_BIAS = "pa_bias"
CONF_AUDIO_LIMITER_LEVEL = "audio_limiter_level"
CONF_SWITCH_MODE = "switch_mode"
CONF_SILENCE_HIGH = "silence_high"
CONF_SILENCE_LOW = "silence_low"
CONF_SILENCE_DETECTION = "silence_detection"
CONF_SILENCE_DURATION = "silence_duration"
CONF_SILENCE_HIGH_COUNTER = "silence_high_counter"
CONF_SILENCE_LOW_COUNTER = "silence_low_counter"
CONF_ALC_GAIN = "alc_gain"
CONF_XTAL_SEL = "xtal_sel"
CONF_AU_ENHANCE = "au_enhance"
CONF_FREQUENCY_DEVIATION = "frequency_deviation"
CONF_REF_CLK = "ref_clk"
CONF_XTAL_ENABLE = "xtal_enable"
CONF_REF_CLK_ENABLE = "ref_clk_enable"
CONF_ALC_HIGH = "alc_high"
CONF_ALC_HOLD_TIME = "alc_hold_time"
CONF_ALC_LOW = "alc_low"

SetFrequencyAction = kt0803_ns.class_(
    "SetFrequencyAction", automation.Action, cg.Parented.template(KT0803Component)
)

ChipId = kt0803_ns.enum("ChipId", True)
CHIP_ID = {
    "KT0803": ChipId.KT0803,
    "KT0803K": ChipId.KT0803K,
    "KT0803M": ChipId.KT0803M,
    "KT0803L": ChipId.KT0803L,
}

PreEmphasis = kt0803_ns.enum("PreEmphasis", True)
PRE_EMPHASIS = {
    "50us": PreEmphasis.PHTCNST_50US,
    "75us": PreEmphasis.PHTCNST_75US,
}

PilotToneAmplitude = kt0803_ns.enum("PilotToneAmplitude", True)
PILOT_TONE_AMPLITUDE = {
    "Low": PilotToneAmplitude.PLTADJ_LOW,
    "High": PilotToneAmplitude.PLTADJ_HIGH,
}

BassBoostControl = kt0803_ns.enum("BassBoostControl", True)
BASS_BOOST_CONTROL = {
    "Disabled": BassBoostControl.BASS_DISABLED,
    "5dB": BassBoostControl.BASS_5DB,
    "11dB": BassBoostControl.BASS_11DB,
    "17dB": BassBoostControl.BASS_17DB,
}

AlcTime = kt0803_ns.enum("AlcTime", True)
ALC_TIME = {
    "25us": AlcTime.ALC_TIME_25US,
    "50us": AlcTime.ALC_TIME_50US,
    "75us": AlcTime.ALC_TIME_75US,
    "100us": AlcTime.ALC_TIME_100US,
    "125us": AlcTime.ALC_TIME_125US,
    "150us": AlcTime.ALC_TIME_150US,
    "175us": AlcTime.ALC_TIME_175US,
    "200us": AlcTime.ALC_TIME_200US,
    "50ms": AlcTime.ALC_TIME_50MS,
    "100ms": AlcTime.ALC_TIME_100MS,
    "150ms": AlcTime.ALC_TIME_150MS,
    "200ms": AlcTime.ALC_TIME_200MS,
    "250ms": AlcTime.ALC_TIME_250MS,
    "300ms": AlcTime.ALC_TIME_300MS,
    "350ms": AlcTime.ALC_TIME_350MS,
    "400ms": AlcTime.ALC_TIME_400MS,
}

SwitchMode = kt0803_ns.enum("SwitchMode", True)
SWITCH_MODE = {
    "Mute": SwitchMode.SW_MOD_MUTE,
    "PA Off": SwitchMode.SW_MOD_PA_OFF,
}

SilenceHigh = kt0803_ns.enum("SilenceHigh", True)
SILENCE_HIGH = {
    "0.5mV": SilenceHigh.SLNCTHH_0M5V,
    "1mV": SilenceHigh.SLNCTHH_1MV,
    "2mV": SilenceHigh.SLNCTHH_2MV,
    "4mV": SilenceHigh.SLNCTHH_4MV,
    "8mV": SilenceHigh.SLNCTHH_8MV,
    "16mV": SilenceHigh.SLNCTHH_16MV,
    "32mV": SilenceHigh.SLNCTHH_32MV,
    "64mV": SilenceHigh.SLNCTHH_64MV,
}

SilenceLow = kt0803_ns.enum("SilenceLow", True)
SILENCE_LOW = {
    "0.25mV": SilenceLow.SLNCTHL_0M25V,
    "0.5mV": SilenceLow.SLNCTHL_0M5V,
    "1mV": SilenceLow.SLNCTHL_1MV,
    "2mV": SilenceLow.SLNCTHL_2MV,
    "4mV": SilenceLow.SLNCTHL_4MV,
    "8mV": SilenceLow.SLNCTHL_8MV,
    "16mV": SilenceLow.SLNCTHL_16MV,
    "32mV": SilenceLow.SLNCTHL_32MV,
}

SilenceLowAndHighLevelDurationTime = kt0803_ns.enum("SilenceLowAndHighLevelDurationTime", True)
SILENCE_LOW_AND_HIGH_LEVEL_DURATION_TIME = {
    "50ms": SilenceLowAndHighLevelDurationTime.SLNCTIME_50MS,
    "100ms": SilenceLowAndHighLevelDurationTime.SLNCTIME_100MS,
    "200ms": SilenceLowAndHighLevelDurationTime.SLNCTIME_200MS,
    "400ms": SilenceLowAndHighLevelDurationTime.SLNCTIME_400MS,
    "1s": SilenceLowAndHighLevelDurationTime.SLNCTIME_1S,
    "2s": SilenceLowAndHighLevelDurationTime.SLNCTIME_2S,
    "4s": SilenceLowAndHighLevelDurationTime.SLNCTIME_4S,
    "8s": SilenceLowAndHighLevelDurationTime.SLNCTIME_8S,
    "16s": SilenceLowAndHighLevelDurationTime.SLNCTIME_16S,
    "24s": SilenceLowAndHighLevelDurationTime.SLNCTIME_24S,
    "32s": SilenceLowAndHighLevelDurationTime.SLNCTIME_32S,
    "40s": SilenceLowAndHighLevelDurationTime.SLNCTIME_40S,
    "48s": SilenceLowAndHighLevelDurationTime.SLNCTIME_48S,
    "56s": SilenceLowAndHighLevelDurationTime.SLNCTIME_56S,
    "60s": SilenceLowAndHighLevelDurationTime.SLNCTIME_60S,
    "64s": SilenceLowAndHighLevelDurationTime.SLNCTIME_64S,
}

SilenceHighLevelCounter = kt0803_ns.enum("SilenceHighLevelCounter", True)
SILENCE_HIGH_LEVEL_COUNTER = {
    "15": SilenceHighLevelCounter.SLNCCNTHIGH_15,
    "31": SilenceHighLevelCounter.SLNCCNTHIGH_31,
    "63": SilenceHighLevelCounter.SLNCCNTHIGH_63,
    "127": SilenceHighLevelCounter.SLNCCNTHIGH_127,
    "255": SilenceHighLevelCounter.SLNCCNTHIGH_255,
    "511": SilenceHighLevelCounter.SLNCCNTHIGH_511,
    "1023": SilenceHighLevelCounter.SLNCCNTHIGH_1023,
    "2047": SilenceHighLevelCounter.SLNCCNTHIGH_2047,
}

AlcCompressedGain = kt0803_ns.enum("AlcCompressedGain", True)
ALC_COMPRESSED_GAIN = {
    "-6dB": AlcCompressedGain.ALCCMPGAIN_N6DB,
    "-9dB": AlcCompressedGain.ALCCMPGAIN_N9DB,
    "-12dB": AlcCompressedGain.ALCCMPGAIN_N12DB,
    "-15dB": AlcCompressedGain.ALCCMPGAIN_N15DB,
    "6dB": AlcCompressedGain.ALCCMPGAIN_6DB,
    "3dB": AlcCompressedGain.ALCCMPGAIN_3DB,
    "0dB": AlcCompressedGain.ALCCMPGAIN_0DB,
    "-3dB": AlcCompressedGain.ALCCMPGAIN_N3DB,
}

SilenceLowLevelCounter = kt0803_ns.enum("SilenceLowLevelCounter", True)
SILENCE_LOW_LEVEL_COUNTER = {
    "1": SilenceLowLevelCounter.SLNCCNTLOW_1,
    "2": SilenceLowLevelCounter.SLNCCNTLOW_2,
    "4": SilenceLowLevelCounter.SLNCCNTLOW_4,
    "8": SilenceLowLevelCounter.SLNCCNTLOW_8,
    "16": SilenceLowLevelCounter.SLNCCNTLOW_16,
    "32": SilenceLowLevelCounter.SLNCCNTLOW_32,
    "64": SilenceLowLevelCounter.SLNCCNTLOW_64,
    "128": SilenceLowLevelCounter.SLNCCNTLOW_128,
}

XtalSel = kt0803_ns.enum("XtalSel", True)
XTAL_SEL = {
    "32.768kHz": XtalSel.XTAL_SEL_32K768HZ,
    "7.6MHz": XtalSel.XTAL_SEL_7M6HZ,
}

FrequencyDeviation = kt0803_ns.enum("FrequencyDeviation", True)
FREQUENCY_DEVIATION = {
    "75kHz": FrequencyDeviation.FDEV_75KHZ,
    "112.5kHz": FrequencyDeviation.FDEV_112K5HZ,
    "150kHz": FrequencyDeviation.FDEV_150KHZ,
    "187.5kHz": FrequencyDeviation.FDEV_187K5HZ,
}

ReferenceClock = kt0803_ns.enum("ReferenceClock", True)
REFERENCE_CLOCK = {
    "32.768kHz": ReferenceClock.REF_CLK_32K768HZ,
    "6.5MHz": ReferenceClock.REF_CLK_6M5HZ,
    "7.6MHz": ReferenceClock.REF_CLK_7M6HZ,
    "12MHz": ReferenceClock.REF_CLK_12MHZ,
    "13MHz": ReferenceClock.REF_CLK_13MHZ,
    "15.2MHz": ReferenceClock.REF_CLK_15M2HZ,
    "19.2MHz": ReferenceClock.REF_CLK_19M2HZ,
    "24MHz": ReferenceClock.REF_CLK_24MHZ,
    "26MHz": ReferenceClock.REF_CLK_26MHZ,
}

AlcHigh = kt0803_ns.enum("AlcHigh", True)
ALC_HIGH = {
    "0.6": AlcHigh.ALCHOLD_06,
    "0.5": AlcHigh.ALCHOLD_05,
    "0.4": AlcHigh.ALCHOLD_04,
    "0.3": AlcHigh.ALCHOLD_03,
    "0.2": AlcHigh.ALCHOLD_02,
    "0.1": AlcHigh.ALCHOLD_01,
    "0.05": AlcHigh.ALCHOLD_005,
    "0.01": AlcHigh.ALCHOLD_001,
}

AlcHoldTime = kt0803_ns.enum("AlcHoldTime", True)
ALC_HOLD_TIME = {
    "50ms": AlcHoldTime.ALCHIGHTH_50MS,
    "100ms": AlcHoldTime.ALCHIGHTH_100MS,
    "150ms": AlcHoldTime.ALCHIGHTH_150MS,
    "200ms": AlcHoldTime.ALCHIGHTH_200MS,
    "1s": AlcHoldTime.ALCHIGHTH_1S,
    "5s": AlcHoldTime.ALCHIGHTH_5S,
    "10s": AlcHoldTime.ALCHIGHTH_10S,
    "15s": AlcHoldTime.ALCHIGHTH_15S,
}

AlcLow = kt0803_ns.enum("AlcLow", True)
ALC_LOW = {
    "0.25": AlcLow.ALCLOWTH_025,
    "0.20": AlcLow.ALCLOWTH_020,
    "0.15": AlcLow.ALCLOWTH_015,
    "0.10": AlcLow.ALCLOWTH_010,
    "0.05": AlcLow.ALCLOWTH_005,
    "0.03": AlcLow.ALCLOWTH_003,
    "0.02": AlcLow.ALCLOWTH_002,
    "0.01": AlcLow.ALCLOWTH_001,
    "0.005": AlcLow.ALCLOWTH_0005,
    "0.001": AlcLow.ALCLOWTH_0001,
    "0.0005": AlcLow.ALCLOWTH_00005,
    "0.0001": AlcLow.ALCLOWTH_00001,
}

AudioLimiterLevel = kt0803_ns.enum("AudioLimiterLevel", True)
AUDIO_LIMITER_LEVEL = {
    "0.6875": AudioLimiterLevel.LMTLVL_06875,
    "0.75": AudioLimiterLevel.LMTLVL_07500,
    "0.875": AudioLimiterLevel.LMTLVL_08750,
    "0.9625": AudioLimiterLevel.LMTLVL_09625,
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(KT0803Component),
            cv.Required(CONF_CHIP_ID): cv.enum(CHIP_ID),
            cv.Optional(CONF_FREQUENCY, default=87.50): cv.float_range(70, 108),
            cv.Optional(CONF_PGA, default=-15): cv.float_range(-15, 12),
            cv.Optional(CONF_RFGAIN, default=108): cv.float_range(95.5, 108),
            cv.Optional(CONF_MUTE, default=False): cv.boolean,
            cv.Optional(CONF_MONO, default=False): cv.boolean,
            cv.Optional(CONF_PRE_EMPHASIS, default="75us"): cv.enum(PRE_EMPHASIS),
            cv.Optional(CONF_PILOT_TONE_AMPLITUDE, default="Low"): cv.enum(PILOT_TONE_AMPLITUDE),
            cv.Optional(CONF_BASS_BOOST_CONTROL, default="Disabled"): cv.enum(BASS_BOOST_CONTROL),
            cv.Optional(CONF_ALC_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_AUTO_PA_DOWN, default=True): cv.boolean,
            cv.Optional(CONF_PA_DOWN, default=False): cv.boolean,
            cv.Optional(CONF_STANDBY_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_ALC_ATTACK_TIME, default="25us"): cv.enum(ALC_TIME),
            cv.Optional(CONF_ALC_DECAY_TIME, default="25us"): cv.enum(ALC_TIME),
            cv.Optional(CONF_PA_BIAS, default=True): cv.boolean,
            cv.Optional(CONF_AUDIO_LIMITER_LEVEL, default="0.875"): cv.enum(AUDIO_LIMITER_LEVEL),
            cv.Optional(CONF_SWITCH_MODE, default="Mute"): cv.enum(SWITCH_MODE),
            cv.Optional(CONF_SILENCE_HIGH, default="32mV"): cv.enum(SILENCE_HIGH),
            cv.Optional(CONF_SILENCE_LOW, default="8mV"): cv.enum(SILENCE_LOW),
            cv.Optional(CONF_SILENCE_DETECTION, default=False): cv.boolean,
            cv.Optional(CONF_SILENCE_DURATION, default="100ms"): cv.enum(SILENCE_LOW_AND_HIGH_LEVEL_DURATION_TIME),
            cv.Optional(CONF_SILENCE_HIGH_COUNTER, default='15'): cv.enum(SILENCE_HIGH_LEVEL_COUNTER),
            cv.Optional(CONF_SILENCE_LOW_COUNTER, default='1'): cv.enum(SILENCE_LOW_LEVEL_COUNTER),
            cv.Optional(CONF_ALC_GAIN, default=-3): cv.float_range(-15, 6),
            cv.Optional(CONF_XTAL_SEL, default="32.768kHz"): cv.enum(XTAL_SEL),
            cv.Optional(CONF_AU_ENHANCE, default=False): cv.boolean,
            cv.Optional(CONF_FREQUENCY_DEVIATION, default="75kHz"): cv.enum(FREQUENCY_DEVIATION),
            cv.Optional(CONF_REF_CLK, default="32.768kHz"): cv.enum(REFERENCE_CLOCK),
            cv.Optional(CONF_XTAL_ENABLE, default=True): cv.boolean,
            cv.Optional(CONF_REF_CLK_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_ALC_HIGH, default="0.6"): cv.enum(ALC_HIGH),
            cv.Optional(CONF_ALC_HOLD_TIME, default="5s"): cv.enum(ALC_HOLD_TIME),
            cv.Optional(CONF_ALC_LOW, default="0.25"): cv.enum(ALC_LOW),
            cv.Optional(CONF_PW_OK): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_POWER,
                icon=ICON_RADIO_TOWER,
            ),
            cv.Optional(CONF_SLNCID): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_EMPTY,
                icon=ICON_VOLUME_MUTE,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x3E))
)


async def set_var(config, id, setter):
    if c := config.get(id):
        cg.add(setter(c))


async def set_binary_sensor(config, id, setter):
    if c := config.get(id):
        s = await binary_sensor.new_binary_sensor(c)
        cg.add(setter(s))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    await set_var(config, CONF_CHIP_ID, var.set_chip_id)
    await set_var(config, CONF_FREQUENCY, var.set_frequency)
    await set_var(config, CONF_PGA, var.set_pga)
    await set_var(config, CONF_RFGAIN, var.set_rfgain)
    await set_var(config, CONF_MUTE, var.set_mute)
    await set_var(config, CONF_MONO, var.set_mono)
    await set_var(config, CONF_PRE_EMPHASIS, var.set_pre_emphasis)
    await set_var(config, CONF_PILOT_TONE_AMPLITUDE, var.set_pilot_tone_amplitude)
    await set_var(config, CONF_BASS_BOOST_CONTROL, var.set_bass_boost_control)    
    await set_var(config, CONF_ALC_ENABLE, var.set_alc_enable)
    await set_var(config, CONF_AUTO_PA_DOWN, var.set_auto_pa_down)
    await set_var(config, CONF_PA_DOWN, var.set_pa_down)
    await set_var(config, CONF_STANDBY_ENABLE, var.set_standby_enable)
    await set_var(config, CONF_ALC_ATTACK_TIME, var.set_alc_attack_time)
    await set_var(config, CONF_ALC_DECAY_TIME, var.set_alc_decay_time)
    await set_var(config, CONF_PA_BIAS, var.set_pa_bias)
    await set_var(config, CONF_AUDIO_LIMITER_LEVEL, var.set_audio_limiter_level)
    await set_var(config, CONF_SWITCH_MODE, var.set_switch_mode)
    await set_var(config, CONF_SILENCE_HIGH, var.set_silence_high)
    await set_var(config, CONF_SILENCE_LOW, var.set_silence_low)
    await set_var(config, CONF_SILENCE_DETECTION, var.set_silence_detection)
    await set_var(config, CONF_SILENCE_DURATION, var.set_silence_duration)
    await set_var(config, CONF_SILENCE_HIGH_COUNTER, var.set_silence_high_counter)
    await set_var(config, CONF_SILENCE_LOW_COUNTER, var.set_silence_low_counter)
    await set_var(config, CONF_ALC_GAIN, var.set_alc_gain)
    await set_var(config, CONF_XTAL_SEL, var.set_xtal_sel)
    await set_var(config, CONF_AU_ENHANCE, var.set_au_enhance)
    await set_var(config, CONF_FREQUENCY_DEVIATION, var.set_frequency_deviation)
    await set_var(config, CONF_REF_CLK, var.set_ref_clk)
    await set_var(config, CONF_XTAL_ENABLE, var.set_xtal_enable)
    await set_var(config, CONF_REF_CLK_ENABLE, var.set_ref_clk_enable)
    await set_var(config, CONF_ALC_HIGH, var.set_alc_high)
    await set_var(config, CONF_ALC_HOLD_TIME, var.set_alc_hold_time)
    await set_var(config, CONF_ALC_LOW, var.set_alc_low)
    await set_binary_sensor(config, CONF_PW_OK, var.set_pw_ok_binary_sensor)
    await set_binary_sensor(config, CONF_SLNCID, var.set_slncid_binary_sensor)
