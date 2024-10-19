import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import i2c, binary_sensor
from esphome.const import (
    CONF_ID,
    CONF_SENSOR,
    CONF_FREQUENCY,
    CONF_GAIN,
    CONF_DURATION,
    CONF_HIGH,
    CONF_LOW,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_EMPTY,
)

CODEOWNERS = ["@gabest11"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "binary_sensor", "number", "switch", "select", "text"]
MULTI_CONF = True

UNIT_MEGA_HERTZ = "MHz"
UNIT_KILO_HERTZ = "kHz"
UNIT_MILLI_VOLT = "mV"
UNIT_MICRO_AMPERE = "mA"
UNIT_DECIBEL_MICRO_VOLT = "dBuV"

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
CONF_REF_CLK = "ref_clk"
CONF_XTAL = "xtal"
CONF_ALC = "alc"
CONF_SILENCE = "silence"
CONF_CHIP_ID = "chip_id"
# general config
# CONF_FREQUENCY = "frequeny"
CONF_DEVIATION = "deviation"
CONF_MUTE = "mute"
CONF_MONO = "mono"
CONF_PRE_EMPHASIS = "pre_emphasis"
CONF_PGA = "pga"
CONF_RFGAIN = "rfgain"
CONF_PILOT_TONE_AMPLITUDE = "pilot_tone_amplitude"
CONF_BASS_BOOST_CONTROL = "bass_boost_control"
CONF_AUTO_PA_DOWN = "auto_pa_down"
CONF_PA_DOWN = "pa_down"
CONF_STANDBY_ENABLE = "standby_enable"
CONF_PA_BIAS = "pa_bias"
CONF_AUDIO_LIMITER_LEVEL = "audio_limiter_level"
CONF_SWITCH_MODE = "switch_mode"
CONF_AU_ENHANCE = "au_enhance"
# ref_clk
CONF_ENABLE = "enable"
CONF_SEL = "sel"
# xtal
# CONF_ENABLE = "enable"
# CONF_SEL = "sel"
# alc
# CONF_ENABLE = "enable"
# CONF_GAIN = "gain"
CONF_ATTACK_TIME = "attack_time"
CONF_DECAY_TIME = "decay_time"
CONF_HOLD_TIME = "hold_time"
# CONF_HIGH = "high"
# CONF_LOW = "low"
# silence
CONF_DETECTION = "detection"
# CONF_DURATION = "duration"
# CONF_HIGH = "high"
# CONF_LOW = "low"
CONF_HIGH_COUNTER = "high_counter"
CONF_LOW_COUNTER = "low_counter"
# sensor
CONF_PW_OK = "pw_ok"
CONF_SLNCID = "slncid"

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

SilenceLowAndHighLevelDurationTime = kt0803_ns.enum(
    "SilenceLowAndHighLevelDurationTime", True
)
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
    "0.6": AlcHigh.ALCHIGHTH_06,
    "0.5": AlcHigh.ALCHIGHTH_05,
    "0.4": AlcHigh.ALCHIGHTH_04,
    "0.3": AlcHigh.ALCHIGHTH_03,
    "0.2": AlcHigh.ALCHIGHTH_02,
    "0.1": AlcHigh.ALCHIGHTH_01,
    "0.05": AlcHigh.ALCHIGHTH_005,
    "0.01": AlcHigh.ALCHIGHTH_001,
}

AlcHoldTime = kt0803_ns.enum("AlcHoldTime", True)
ALC_HOLD_TIME = {
    "50ms": AlcHoldTime.ALCHOLD_50MS,
    "100ms": AlcHoldTime.ALCHOLD_100MS,
    "150ms": AlcHoldTime.ALCHOLD_150MS,
    "200ms": AlcHoldTime.ALCHOLD_200MS,
    "1s": AlcHoldTime.ALCHOLD_1S,
    "5s": AlcHoldTime.ALCHOLD_5S,
    "10s": AlcHoldTime.ALCHOLD_10S,
    "15s": AlcHoldTime.ALCHOLD_15S,
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

REF_CLK_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE, default=False): cv.boolean,
        cv.Optional(CONF_SEL, default="32.768kHz"): cv.enum(REFERENCE_CLOCK),
    }
)

XTAL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE, default=True): cv.boolean,
        cv.Optional(CONF_SEL, default="32.768kHz"): cv.enum(XTAL_SEL),
    }
)

ALC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE, default=False): cv.boolean,
        cv.Optional(CONF_GAIN, default=-3): cv.float_range(-15, 6),
        cv.Optional(CONF_ATTACK_TIME, default="25us"): cv.enum(ALC_TIME),
        cv.Optional(CONF_DECAY_TIME, default="25us"): cv.enum(ALC_TIME),
        cv.Optional(CONF_HOLD_TIME, default="5s"): cv.enum(ALC_HOLD_TIME),
        cv.Optional(CONF_HIGH, default="0.6"): cv.enum(ALC_HIGH),
        cv.Optional(CONF_LOW, default="0.25"): cv.enum(ALC_LOW),
    }
)

SILENCE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DETECTION, default=False): cv.boolean,
        cv.Optional(CONF_DURATION, default="100ms"): cv.enum(
            SILENCE_LOW_AND_HIGH_LEVEL_DURATION_TIME
        ),
        cv.Optional(CONF_HIGH, default="32mV"): cv.enum(SILENCE_HIGH),
        cv.Optional(CONF_LOW, default="8mV"): cv.enum(SILENCE_LOW),
        cv.Optional(CONF_HIGH_COUNTER, default="15"): cv.enum(
            SILENCE_HIGH_LEVEL_COUNTER
        ),
        cv.Optional(CONF_LOW_COUNTER, default="1"): cv.enum(SILENCE_LOW_LEVEL_COUNTER),
    }
)

SENSOR_SCHEMA = cv.Schema(
    {
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

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(KT0803Component),
            cv.Required(CONF_CHIP_ID): cv.enum(CHIP_ID),
            cv.Optional(CONF_FREQUENCY, default=87.50): cv.float_range(70, 108),
            cv.Optional(CONF_DEVIATION, default="75kHz"): cv.enum(FREQUENCY_DEVIATION),
            cv.Optional(CONF_MUTE, default=False): cv.boolean,
            cv.Optional(CONF_MONO, default=False): cv.boolean,
            cv.Optional(CONF_PRE_EMPHASIS, default="75us"): cv.enum(PRE_EMPHASIS),
            cv.Optional(CONF_PGA, default=0): cv.float_range(-15, 12),
            cv.Optional(CONF_RFGAIN, default=108): cv.float_range(95.5, 108),
            cv.Optional(CONF_PILOT_TONE_AMPLITUDE, default="Low"): cv.enum(
                PILOT_TONE_AMPLITUDE
            ),
            cv.Optional(CONF_BASS_BOOST_CONTROL, default="Disabled"): cv.enum(
                BASS_BOOST_CONTROL
            ),
            cv.Optional(CONF_AUTO_PA_DOWN, default=True): cv.boolean,
            cv.Optional(CONF_PA_DOWN, default=False): cv.boolean,
            cv.Optional(CONF_STANDBY_ENABLE, default=False): cv.boolean,
            cv.Optional(CONF_PA_BIAS, default=True): cv.boolean,
            cv.Optional(CONF_AUDIO_LIMITER_LEVEL, default="0.875"): cv.enum(
                AUDIO_LIMITER_LEVEL
            ),
            cv.Optional(CONF_SWITCH_MODE, default="Mute"): cv.enum(SWITCH_MODE),
            cv.Optional(CONF_AU_ENHANCE, default=False): cv.boolean,
            cv.Optional(CONF_REF_CLK): REF_CLK_SCHEMA,
            cv.Optional(CONF_XTAL): XTAL_SCHEMA,
            cv.Optional(CONF_ALC): ALC_SCHEMA,
            cv.Optional(CONF_SILENCE): SILENCE_SCHEMA,
            cv.Optional(CONF_SENSOR): SENSOR_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x3E))
)

VARIABLES = {
    None: [
        [CONF_CHIP_ID],
        [CONF_FREQUENCY],
        [CONF_DEVIATION],
        [CONF_MUTE],
        [CONF_MONO],
        [CONF_PRE_EMPHASIS],
        [CONF_PGA],
        [CONF_RFGAIN],
        [CONF_PILOT_TONE_AMPLITUDE],
        [CONF_BASS_BOOST_CONTROL],
        [CONF_AUTO_PA_DOWN],
        [CONF_PA_DOWN],
        [CONF_STANDBY_ENABLE],
        [CONF_PA_BIAS],
        [CONF_AUDIO_LIMITER_LEVEL],
        [CONF_SWITCH_MODE],
        [CONF_AU_ENHANCE],
    ],
    CONF_REF_CLK: [
        [CONF_ENABLE],
        [CONF_SEL],
    ],
    CONF_XTAL: [
        [CONF_ENABLE],
        [CONF_SEL],
    ],
    CONF_ALC: [
        [CONF_ENABLE],
        [CONF_GAIN],
        [CONF_ATTACK_TIME],
        [CONF_DECAY_TIME],
        [CONF_HOLD_TIME],
        [CONF_HIGH],
        [CONF_LOW],
    ],
    CONF_SILENCE: [
        [CONF_DETECTION],
        [CONF_DURATION],
        [CONF_HIGH],
        [CONF_LOW],
        [CONF_HIGH_COUNTER],
        [CONF_LOW_COUNTER],
    ],
}

SENSORS = {
    CONF_SENSOR: [
        [CONF_PW_OK, "binary_sensor"],
        [CONF_SLNCID, "binary_sensor"],
    ]
}


async def for_each_conf(config, vars, callback):
    for section in vars:
        c = config[section] if section in config else config
        for args in vars[section]:
            setter = "set_"
            if section is not None and section != CONF_SENSOR:
                setter += section + "_"
            setter += args[0]
            if cc := c.get(args[0]):
                await callback(cc, args, setter)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    async def set_var(c, a, s):
        cg.add(getattr(var, s)(c))

    await for_each_conf(config, VARIABLES, set_var)

    async def new_sensor(c, args, setter):
        s = None
        if args[1] == "binary_sensor":
            s = await binary_sensor.new_binary_sensor(c)
        cg.add(getattr(var, setter + "_" + args[1])(s))

    await for_each_conf(config, SENSORS, new_sensor)


FREQUENCY_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(KT0803Component),
        cv.Required(CONF_FREQUENCY): cv.float_range(min=70, max=108),
    }
)

SetFrequencyAction = kt0803_ns.class_(
    "SetFrequencyAction", automation.Action, cg.Parented.template(KT0803Component)
)


@automation.register_action(
    "kt0803.set_frequency", SetFrequencyAction, FREQUENCY_SCHEMA
)
async def tune_frequency_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if frequency := config.get(CONF_FREQUENCY):
        template_ = await cg.templatable(frequency, args, cg.float_)
        cg.add(var.set_frequency(template_))
    return var
