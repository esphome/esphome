import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation, pins
from esphome.components import i2c, sensor, binary_sensor, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_FREQUENCY,
    STATE_CLASS_MEASUREMENT,
    ICON_CHIP,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

CODEOWNERS = ["@gabest11"]
DEPENDENCIES = ["i2c"]
AUTO_LOAD = ["sensor", "text_sensor", "number", "switch", "select", "text"]
MULTI_CONF = True

UNIT_MEGA_HERTZ = "MHz"
UNIT_KILO_HERTZ = "kHz"
UNIT_MILLI_VOLT = "mV"
UNIT_MICRO_AMPERE = "mA"
UNIT_DECIBEL_MICRO_VOLT = "dBµV"
UNIT_PICO_FARAD = "pF"
UNIT_KILO_OHM = "kOhm"
UNIT_DECIBEL_FS = "dBfs"

ICON_VOLUME_MUTE = "mdi:volume-mute"
ICON_EAR_HEARING = "mdi:ear-hearing"
ICON_RADIO_TOWER = "mdi:radio-tower"
ICON_SLEEP = "mdi:sleep"
ICON_SINE_WAVE = "mdi:sine-wave"
ICON_RESISTOR = "mdi:resistor"
ICON_FORMAT_TEXT = "mdi:format-text"

si4713_ns = cg.esphome_ns.namespace("si4713")
Si4713Component = si4713_ns.class_(
    "Si4713Component", cg.PollingComponent, i2c.I2CDevice
)

CONF_SI4713_ID = "si4713_id"
CONF_RESET_PIN = "reset_pin"
CONF_SECTION_TUNER = "tuner"
CONF_SECTION_ANALOG = "analog"
CONF_SECTION_DIGITAL = "digital"
CONF_SECTION_PILOT = "pilot"
CONF_SECTION_REFCLK = "refclk"
CONF_SECTION_COMPRESSOR = "compressor"
CONF_SECTION_LIMITER = "limiter"
CONF_SECTION_ASQ = "asq"
CONF_SECTION_RDS = "rds"
CONF_SECTION_OUTPUT = "output"
CONF_SECTION_SENSOR = "sensor"
# general config
CONF_OP_MODE = "op_mode"
CONF_MUTE = "mute"
CONF_MONO = "mono"
CONF_PRE_EMPHASIS = "pre_emphasis"
# tuner
# CONF_FREQUENCY = "frequency"
CONF_DEVIATION = "deviation"
CONF_POWER = "power"
CONF_ANTCAP = "antcap"
# analog
CONF_LEVEL = "level"
CONF_ATTENUATION = "attenuation"
# digital
CONF_SAMPLE_RATE = "sample_rate"
CONF_SAMPLE_BITS = "sample_bits"
CONF_CHANNELS = "channels"
CONF_MODE = "mode"
CONF_CLOCK_EDGE = "clock_edge"
# pilot
CONF_ENABLE = "enable"
# CONF_FREQUENCY = "frequency"
CONF_DEVIATION = "deviation"
# refclk
# CONF_FREQUENCY = "frequency"
CONF_SOURCE = "source"
CONF_PRESCALER = "prescaler"
# compressor
CONF_ENABLE = "enable"
CONF_THRESHOLD = "threshold"
CONF_ATTACK = "attack"
CONF_RELEASE = "release"
CONF_GAIN = "gain"
CONF_PRESET = "preset"
# limiter
CONF_ENABLE = "enable"
CONF_RELEASE_TIME = "release_time"
# asq
CONF_IALL = "iall"
CONF_IALH = "ialh"
CONF_OVERMOD = "overmod"
CONF_LEVEL_LOW = "level_low"
CONF_DURATION_LOW = "duration_low"
CONF_LEVEL_HIGH = "level_high"
CONF_DURATION_HIGH = "duration_high"
# rds
CONF_ENABLE = "enable"
CONF_DEVIATION = "deviation"
CONF_STATION = "station"
CONF_TEXT = "text"
# output
CONF_GPIO1 = "gpio1"
CONF_GPIO2 = "gpio2"
CONF_GPIO3 = "gpio3"
# sensor
CONF_CHIP_ID = "chip_id"
CONF_READ_FREQUENCY = "frequency"
CONF_READ_POWER = "power"
CONF_READ_ANTCAP = "antcap"
CONF_READ_NOISE_LEVEL = "noise_level"
CONF_IALL = "iall"
CONF_IALH = "ialh"
CONF_OVERMOD = "overmod"
CONF_INLEVEL = "inlevel"

SetFrequencyAction = si4713_ns.class_(
    "SetFrequencyAction", automation.Action, cg.Parented.template(Si4713Component)
)

MeasureFrequencyAction = si4713_ns.class_(
    "MeasureFrequencyAction", automation.Action, cg.Parented.template(Si4713Component)
)

OpMode = si4713_ns.enum("OpMode", True)
OP_MODE = {
    "Analog": OpMode.OPMODE_ANALOG,
    "Digital": OpMode.OPMODE_Digital,
}

PreEmphasis = si4713_ns.enum("PreEmphasis", True)
PRE_EMPHASIS = {
    "75us": PreEmphasis.FMPE_75US,
    "50us": PreEmphasis.FMPE_50US,
    "Disabled": PreEmphasis.FMPE_DISABLED,
}

LineAttenuation = si4713_ns.enum("LineAttenuation", True)
LINE_ATTENUATION = {
    "396kOhm": LineAttenuation.LIATTEN_396KOHM,
    "100kOhm": LineAttenuation.LIATTEN_100KOHM,
    "74kOhm": LineAttenuation.LIATTEN_74KOHM,
    "60kOhm": LineAttenuation.LIATTEN_60KOHM,
}

SampleBits = si4713_ns.enum("SampleBits", True)
SAMPLE_BITS = {
    "16": SampleBits.ISIZE_16BITS,
    "20": SampleBits.ISIZE_20BITS,
    "24": SampleBits.ISIZE_24BITS,
    "8": SampleBits.ISIZE_8BITS,
}

SampleChannels = si4713_ns.enum("SampleChannels", True)
SAMPLE_CHANNELS = {
    "Stereo": SampleChannels.IMONO_STEREO,
    "Mono": SampleChannels.IMONO_MONO,
}

DigitalMode = si4713_ns.enum("DigitalMode", True)
DIGITAL_MODE = {
    "Default": DigitalMode.IMODE_DEFAULT,
    "I2S": DigitalMode.IMODE_I2S,
    "Left Justified": DigitalMode.IMODE_LEFT_JUSTIFIED,
    "MSB at 1st": DigitalMode.IMODE_MSB_AT_1ST,
    "MSB at 2nd": DigitalMode.IMODE_MSB_AT_2ND,
}

DigitalClockEdge = si4713_ns.enum("DigitalClockEdge", True)
DIGITAL_CLOCK_EDGE = {
    "Rising": DigitalClockEdge.IFALL_DCLK_RISING_EDGE,
    "Falling": DigitalClockEdge.IFALL_DCLK_FALLING_EDGE,
}

RefClkSource = si4713_ns.enum("RefClkSource", True)
REFCLK_SOURCE = {
    "RCLK": RefClkSource.RCLKSEL_RCLK,
    "DCLK": RefClkSource.RCLKSEL_DCLK,
}

AcompAttack = si4713_ns.enum("AcompAttack", True)
ACOMP_ATTACK = {
    "0.5ms": AcompAttack.ATTACK_05MS,
    "1.0ms": AcompAttack.ATTACK_10MS,
    "1.5ms": AcompAttack.ATTACK_15MS,
    "2.0ms": AcompAttack.ATTACK_20MS,
    "2.5ms": AcompAttack.ATTACK_25MS,
    "3.0ms": AcompAttack.ATTACK_30MS,
    "3.5ms": AcompAttack.ATTACK_35MS,
    "4.0ms": AcompAttack.ATTACK_40MS,
    "4.5ms": AcompAttack.ATTACK_45MS,
    "5.0ms": AcompAttack.ATTACK_50MS,
}

AcompRelease = si4713_ns.enum("AcompRelease", True)
ACOMP_RELEASE = {
    "100ms": AcompRelease.RELEASE_100MS,
    "200ms": AcompRelease.RELEASE_200MS,
    "350ms": AcompRelease.RELEASE_350MS,
    "525ms": AcompRelease.RELEASE_525MS,
    "1000ms": AcompRelease.RELEASE_1000MS,
}

AcompPreset = si4713_ns.enum("AcompPreset", True)
ACOMP_PRESET = {
    "Minimal": AcompPreset.ACOMP_MINIMAL,
    "Aggressive": AcompPreset.ACOMP_AGGRESSIVE,
    "Custom": AcompPreset.ACOMP_CUSTOM,
}

TUNER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FREQUENCY, default=87.50): cv.float_range(76, 108),  # MHz
        cv.Optional(CONF_DEVIATION, default=68.25): cv.float_range(0, 90),  # kHz
        cv.Optional(CONF_POWER, default=115): cv.int_range(88, 115),
        cv.Optional(CONF_ANTCAP, default=0): cv.int_range(0, 191),
    }
)

ANALOG_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_LEVEL, default=636): cv.int_range(0, 1023),
        cv.Optional(CONF_ATTENUATION, default=636): cv.enum(LINE_ATTENUATION),
    }
)

DIGITAL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SAMPLE_RATE, default=44100): cv.int_range(32000, 48000),  # Hz
        cv.Optional(CONF_SAMPLE_BITS, default="16"): cv.enum(SAMPLE_BITS),
        cv.Optional(CONF_CHANNELS, default="Stereo"): cv.enum(SAMPLE_CHANNELS),
        cv.Optional(CONF_MODE, default="Default"): cv.enum(DIGITAL_MODE),
        cv.Optional(CONF_CLOCK_EDGE, default="Rising"): cv.enum(DIGITAL_CLOCK_EDGE),
    }
)

PILOT_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE, default="True"): cv.boolean,
        cv.Optional(CONF_FREQUENCY, default=19): cv.float_range(0, 19),  # kHz
        cv.Optional(CONF_DEVIATION, default=6.75): cv.float_range(0, 90),  # kHz
    }
)

REFCLK_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_FREQUENCY, default=32768): cv.int_range(31130, 34406),  # Hz
        cv.Optional(CONF_SOURCE, default="RCLK"): cv.enum(REFCLK_SOURCE),
        cv.Optional(CONF_PRESCALER, default=1): cv.int_range(0, 4095),
    }
)

COMPRESSOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE, default="False"): cv.boolean,
        cv.Optional(CONF_THRESHOLD, default=-40): cv.int_range(-40, 0),
        cv.Optional(CONF_ATTACK, default="0.5"): cv.enum(ACOMP_ATTACK),
        cv.Optional(CONF_RELEASE, default="1000"): cv.enum(ACOMP_RELEASE),
        cv.Optional(CONF_GAIN, default=15): cv.int_range(0, 20),
        cv.Optional(CONF_PRESET, default="Custom"): cv.enum(ACOMP_PRESET),
    }
)

LIMITER_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE, default="True"): cv.boolean,
        cv.Optional(CONF_RELEASE_TIME, default=5): cv.float_range(0.25, 102.4),
    }
)

ASQ_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_IALL, default="False"): cv.boolean,
        cv.Optional(CONF_IALH, default="False"): cv.boolean,
        cv.Optional(CONF_OVERMOD, default="False"): cv.boolean,
        cv.Optional(CONF_LEVEL_LOW, default=-50): cv.float_range(-70, 0),
        cv.Optional(CONF_DURATION_LOW, default=10000): cv.int_range(0, 65535),
        cv.Optional(CONF_LEVEL_HIGH, default=-20): cv.float_range(-70, 0),
        cv.Optional(CONF_DURATION_HIGH, default=5000): cv.int_range(0, 65535),
    }
)

RDS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_ENABLE, default=False): cv.boolean,
        cv.Optional(CONF_DEVIATION, default=2.0): cv.float_range(0, 7.5),  # kHz
        cv.Optional(CONF_STATION): cv.string,
        cv.Optional(CONF_TEXT): cv.string,
    }
)

SENSOR_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CHIP_ID): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            icon=ICON_CHIP,
        ),
        cv.Optional(CONF_READ_FREQUENCY): sensor.sensor_schema(
            unit_of_measurement=UNIT_MEGA_HERTZ,
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            state_class=STATE_CLASS_MEASUREMENT,
            accuracy_decimals=2,
            # icon=ICON_,
        ),
        cv.Optional(CONF_READ_POWER): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MICRO_VOLT,
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon=ICON_,
        ),
        cv.Optional(CONF_READ_ANTCAP): sensor.sensor_schema(
            unit_of_measurement=UNIT_PICO_FARAD,
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon=ICON_,
        ),
        cv.Optional(CONF_READ_NOISE_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_MICRO_VOLT,
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            # state_class=STATE_CLASS_MEASUREMENT,
            # icon=ICON_,
        ),
        cv.Optional(CONF_IALL): binary_sensor.binary_sensor_schema(
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            # icon=ICON_,
        ),
        cv.Optional(CONF_IALH): binary_sensor.binary_sensor_schema(
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            # icon=ICON_,
        ),
        cv.Optional(CONF_OVERMOD): binary_sensor.binary_sensor_schema(
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            # icon=ICON_,
        ),
        cv.Optional(CONF_INLEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_DECIBEL_FS,
            # entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            state_class=STATE_CLASS_MEASUREMENT,
            # icon=ICON_,
        ),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Si4713Component),
            cv.Required(CONF_RESET_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_OP_MODE, default="Analog"): cv.enum(OP_MODE),
            cv.Optional(CONF_MUTE, default=False): cv.boolean,
            cv.Optional(CONF_MONO, default=False): cv.boolean,
            cv.Optional(CONF_PRE_EMPHASIS, default="75us"): cv.enum(PRE_EMPHASIS),
            cv.Optional(CONF_SECTION_TUNER): TUNER_SCHEMA,
            cv.Optional(CONF_SECTION_ANALOG): ANALOG_SCHEMA,
            cv.Optional(CONF_SECTION_DIGITAL): DIGITAL_SCHEMA,
            cv.Optional(CONF_SECTION_PILOT): PILOT_SCHEMA,
            cv.Optional(CONF_SECTION_REFCLK): REFCLK_SCHEMA,
            cv.Optional(CONF_SECTION_COMPRESSOR): COMPRESSOR_SCHEMA,
            cv.Optional(CONF_SECTION_LIMITER): LIMITER_SCHEMA,
            cv.Optional(CONF_SECTION_ASQ): ASQ_SCHEMA,
            cv.Optional(CONF_SECTION_RDS): RDS_SCHEMA,
            cv.Optional(CONF_SECTION_SENSOR): SENSOR_SCHEMA,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x63))
)


FREQUENCY_SCHEMA = automation.maybe_simple_id(
    {
        cv.GenerateID(): cv.use_id(Si4713Component),
        cv.Required(CONF_FREQUENCY): cv.templatable(cv.float_range(min=76, max=108)),
    }
)


@automation.register_action(
    "si4713.set_frequency", SetFrequencyAction, FREQUENCY_SCHEMA
)
@automation.register_action(
    "si4713.measure_frequency", MeasureFrequencyAction, FREQUENCY_SCHEMA
)
async def tune_frequency_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    if frequency := config.get(CONF_FREQUENCY):
        template_ = await cg.templatable(frequency, args, cg.float_)
        cg.add(var.set_frequency(template_))
    return var


async def set_var(config, id, setter):
    if c := config.get(id):
        cg.add(setter(c))


async def new_sensor(config, id, setter):
    if c := config.get(id):
        s = await sensor.new_sensor(c)
        cg.add(setter(s))


async def new_binary_sensor(config, id, setter):
    if c := config.get(id):
        s = await binary_sensor.new_binary_sensor(c)
        cg.add(setter(s))


async def new_text_sensor(config, id, setter):
    if c := config.get(id):
        s = await text_sensor.new_text_sensor(c)
        cg.add(setter(s))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
    cg.add(var.set_reset_pin(reset_pin))
    await set_var(config, CONF_OP_MODE, var.set_op_mode)
    await set_var(config, CONF_MUTE, var.set_mute)
    await set_var(config, CONF_MONO, var.set_mono)
    await set_var(config, CONF_PRE_EMPHASIS, var.set_pre_emphasis)
    if tuner_config := config.get(CONF_SECTION_TUNER):
        await set_var(tuner_config, CONF_FREQUENCY, var.set_frequency)
        await set_var(tuner_config, CONF_DEVIATION, var.set_audio_deviation)
        await set_var(tuner_config, CONF_POWER, var.set_power)
        await set_var(tuner_config, CONF_ANTCAP, var.set_antcap)
    if analog_config := config.get(CONF_SECTION_ANALOG):
        await set_var(analog_config, CONF_LEVEL, var.set_analog_level)
        await set_var(analog_config, CONF_ATTENUATION, var.set_analog_attenuation)
    if digital_config := config.get(CONF_SECTION_DIGITAL):
        await set_var(digital_config, CONF_SAMPLE_RATE, var.set_digital_sample_rate)
        await set_var(digital_config, CONF_SAMPLE_BITS, var.set_digital_sample_bits)
        await set_var(digital_config, CONF_CHANNELS, var.set_digital_channels)
        await set_var(digital_config, CONF_MODE, var.set_digital_mode)
        await set_var(digital_config, CONF_CLOCK_EDGE, var.set_digital_clock_edge)
    if pilot_config := config.get(CONF_SECTION_PILOT):
        await set_var(pilot_config, CONF_ENABLE, var.set_pilot_enable)
        await set_var(pilot_config, CONF_FREQUENCY, var.set_pilot_frequency)
        await set_var(pilot_config, CONF_DEVIATION, var.set_pilot_deviation)
    if refclk_config := config.get(CONF_SECTION_REFCLK):
        await set_var(refclk_config, CONF_FREQUENCY, var.set_refclk_frequency)
        await set_var(refclk_config, CONF_SOURCE, var.set_refclk_source)
        await set_var(refclk_config, CONF_PRESCALER, var.set_refclk_prescaler)
    if compressor_config := config.get(CONF_SECTION_COMPRESSOR):
        await set_var(compressor_config, CONF_ENABLE, var.set_acomp_enable)
        await set_var(compressor_config, CONF_THRESHOLD, var.set_acomp_threshold)
        await set_var(compressor_config, CONF_ATTACK, var.set_acomp_attack)
        await set_var(compressor_config, CONF_RELEASE, var.set_acomp_release)
        await set_var(compressor_config, CONF_GAIN, var.set_acomp_gain)
        await set_var(compressor_config, CONF_PRESET, var.set_acomp_preset)
    if limiter_config := config.get(CONF_SECTION_LIMITER):
        await set_var(limiter_config, CONF_ENABLE, var.set_limiter_enable)
        await set_var(limiter_config, CONF_RELEASE_TIME, var.set_limiter_release_time)
    if asq_config := config.get(CONF_SECTION_ASQ):
        await set_var(asq_config, CONF_IALL, var.set_asq_iall_enable)
        await set_var(asq_config, CONF_IALH, var.set_asq_ialh_enable)
        await set_var(asq_config, CONF_OVERMOD, var.set_asq_overmod_enable)
        await set_var(asq_config, CONF_LEVEL_LOW, var.set_asq_level_low)
        await set_var(asq_config, CONF_DURATION_LOW, var.set_asq_duration_low)
        await set_var(asq_config, CONF_LEVEL_HIGH, var.set_asq_level_high)
        await set_var(asq_config, CONF_DURATION_HIGH, var.set_asq_duration_high)
    if rds_config := config.get(CONF_SECTION_RDS):
        await set_var(rds_config, CONF_ENABLE, var.set_rds_enable)
        await set_var(rds_config, CONF_DEVIATION, var.set_rds_deviation)
        await set_var(rds_config, CONF_STATION, var.set_rds_station)
        await set_var(rds_config, CONF_TEXT, var.set_rds_text)
    if sensor_config := config.get(CONF_SECTION_SENSOR):
        await new_text_sensor(sensor_config, CONF_CHIP_ID, var.set_chip_id_text_sensor)
        await new_sensor(
            sensor_config, CONF_READ_FREQUENCY, var.set_read_frequency_sensor
        )
        await new_sensor(sensor_config, CONF_READ_POWER, var.set_read_power_sensor)
        await new_sensor(sensor_config, CONF_READ_ANTCAP, var.set_read_antcap_sensor)
        await new_sensor(
            sensor_config, CONF_READ_NOISE_LEVEL, var.set_read_noise_level_sensor
        )
        await new_binary_sensor(sensor_config, CONF_IALL, var.set_iall_binary_sensor)
        await new_binary_sensor(sensor_config, CONF_IALH, var.set_ialh_binary_sensor)
        await new_binary_sensor(
            sensor_config, CONF_OVERMOD, var.set_overmod_binary_sensor
        )
        await new_sensor(sensor_config, CONF_INLEVEL, var.set_inlevel_sensor)
