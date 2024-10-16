import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_MODE,
    UNIT_HERTZ,
    UNIT_MILLISECOND,
    UNIT_DECIBEL,
    UNIT_EMPTY,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_EMPTY,
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    CONF_SI4713_ID,
    Si4713Component,
    si4713_ns,
    CONF_SECTION_TUNER,
    CONF_SECTION_ANALOG,
    CONF_SECTION_DIGITAL,
    CONF_SECTION_PILOT,
    CONF_SECTION_REFCLK,
    CONF_SECTION_COMPRESSOR,
    CONF_SECTION_LIMITER,
    CONF_SECTION_ASQ,
    CONF_SECTION_RDS,
    CONF_FREQUENCY,
    CONF_DEVIATION,
    CONF_POWER,
    CONF_ANTCAP,
    CONF_LEVEL,
    CONF_SAMPLE_RATE,
    CONF_PRESCALER,
    CONF_THRESHOLD,
    CONF_GAIN,
    CONF_RELEASE_TIME,
    CONF_LEVEL_LOW,
    CONF_DURATION_LOW,
    CONF_LEVEL_HIGH,
    CONF_DURATION_HIGH,
    UNIT_MEGA_HERTZ,
    UNIT_KILO_HERTZ,
    UNIT_MILLI_VOLT,
    UNIT_DECIBEL_MICRO_VOLT,
    UNIT_PICO_FARAD,
)

FrequencyNumber = si4713_ns.class_("FrequencyNumber", number.Number)
AudioDeviationNumber = si4713_ns.class_("AudioDeviationNumber", number.Number)
PowerNumber = si4713_ns.class_("PowerNumber", number.Number)
AntcapNumber = si4713_ns.class_("AntcapNumber", number.Number)
AnalogLevelNumber = si4713_ns.class_("AnalogLevelNumber", number.Number)
DigitalSampleRateNumber = si4713_ns.class_("DigitalSampleRateNumber", number.Number)
PilotFrequencyNumber = si4713_ns.class_("PilotFrequencyNumber", number.Number)
PilotDeviationNumber = si4713_ns.class_("PilotDeviationNumber", number.Number)
RefClkFrequencyNumber = si4713_ns.class_("RefClkFrequencyNumber", number.Number)
RefClkPrescalerNumber = si4713_ns.class_("RefClkPrescalerNumber", number.Number)
AcompThresholdNumber = si4713_ns.class_("AcompThresholdNumber", number.Number)
AcompGainNumber = si4713_ns.class_("AcompGainNumber", number.Number)
LimiterReleaseTimeNumber = si4713_ns.class_("LimiterReleaseTimeNumber", number.Number)
AsqLevelLowNumber = si4713_ns.class_("AsqLevelLowNumber", number.Number)
AsqDurationLowNumber = si4713_ns.class_("AsqDurationLowNumber", number.Number)
AsqLevelHighNumber = si4713_ns.class_("AsqLevelHighNumber", number.Number)
AsqDurationHighNumber = si4713_ns.class_("AsqDurationHighNumber", number.Number)
RdsDeviationNumber = si4713_ns.class_("RdsDeviationNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SI4713_ID): cv.use_id(Si4713Component),
        cv.Optional(CONF_SECTION_TUNER): cv.Schema(
            {
                cv.Optional(CONF_FREQUENCY): number.number_schema(
                    FrequencyNumber,
                    unit_of_measurement=UNIT_MEGA_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_DEVIATION): number.number_schema(
                    AudioDeviationNumber,
                    unit_of_measurement=UNIT_KILO_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_POWER): number.number_schema(
                    PowerNumber,
                    unit_of_measurement=UNIT_DECIBEL_MICRO_VOLT,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_ANTCAP): number.number_schema(
                    AntcapNumber,
                    unit_of_measurement=UNIT_PICO_FARAD,
                    device_class=DEVICE_CLASS_EMPTY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            }
        ),
        cv.Optional(CONF_SECTION_ANALOG): cv.Schema(
            {
                cv.Optional(CONF_LEVEL): number.number_schema(
                    AnalogLevelNumber,
                    unit_of_measurement=UNIT_MILLI_VOLT,
                    device_class=DEVICE_CLASS_VOLTAGE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            },
        ),
        cv.Optional(CONF_SECTION_DIGITAL): cv.Schema(
            {
                cv.Optional(CONF_SAMPLE_RATE): number.number_schema(
                    DigitalSampleRateNumber,
                    unit_of_measurement=UNIT_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            },
        ),
        cv.Optional(CONF_SECTION_PILOT): cv.Schema(
            {
                cv.Optional(CONF_FREQUENCY): number.number_schema(
                    PilotFrequencyNumber,
                    unit_of_measurement=UNIT_KILO_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_DEVIATION): number.number_schema(
                    PilotDeviationNumber,
                    unit_of_measurement=UNIT_KILO_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            },
        ),
        cv.Optional(CONF_SECTION_REFCLK): cv.Schema(
            {
                cv.Optional(CONF_FREQUENCY): number.number_schema(
                    RefClkFrequencyNumber,
                    unit_of_measurement=UNIT_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_PRESCALER): number.number_schema(
                    RefClkPrescalerNumber,
                    unit_of_measurement=UNIT_EMPTY,
                    device_class=DEVICE_CLASS_EMPTY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            },
        ),
        cv.Optional(CONF_SECTION_COMPRESSOR): cv.Schema(
            {
                cv.Optional(CONF_THRESHOLD): number.number_schema(
                    AcompThresholdNumber,
                    unit_of_measurement=UNIT_DECIBEL,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_GAIN): number.number_schema(
                    AcompGainNumber,
                    unit_of_measurement=UNIT_DECIBEL,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            },
        ),
        cv.Optional(CONF_SECTION_LIMITER): cv.Schema(
            {
                cv.Optional(CONF_RELEASE_TIME): number.number_schema(
                    LimiterReleaseTimeNumber,
                    unit_of_measurement=UNIT_MILLISECOND,
                    device_class=DEVICE_CLASS_DURATION,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            },
        ),
        cv.Optional(CONF_SECTION_ASQ): cv.Schema(
            {
                cv.Optional(CONF_LEVEL_LOW): number.number_schema(
                    AsqLevelLowNumber,
                    unit_of_measurement=UNIT_DECIBEL,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_DURATION_LOW): number.number_schema(
                    AsqDurationLowNumber,
                    unit_of_measurement=UNIT_MILLISECOND,
                    device_class=DEVICE_CLASS_DURATION,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_LEVEL_HIGH): number.number_schema(
                    AsqLevelHighNumber,
                    unit_of_measurement=UNIT_DECIBEL,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_DURATION_HIGH): number.number_schema(
                    AsqDurationHighNumber,
                    unit_of_measurement=UNIT_MILLISECOND,
                    device_class=DEVICE_CLASS_DURATION,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            }
        ),
        cv.Optional(CONF_SECTION_RDS): cv.Schema(
            {
                cv.Optional(CONF_DEVIATION): number.number_schema(
                    RdsDeviationNumber,
                    unit_of_measurement=UNIT_KILO_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
             },
         ),
    }
)


async def new_number(parent, config, id, setter, min_value, max_value, step, *args, **kwargs):
    if c := config.get(id):
        if CONF_MODE in kwargs:
            if CONF_MODE not in c or c[CONF_MODE] == number.NumberMode.NUMBER_MODE_AUTO:
                c[CONF_MODE] = kwargs.get(CONF_MODE)
        n = await number.new_number(
            c, *args, min_value=min_value, max_value=max_value, step=step
        )
        await cg.register_parented(n, parent)
        cg.add(setter(n))
        return n


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SI4713_ID])
    if tuner_config := config.get(CONF_SECTION_TUNER):
        await new_number(parent, tuner_config, CONF_FREQUENCY, parent.set_frequency_number, 76, 108, 0.05)
        await new_number(parent, tuner_config, CONF_DEVIATION, parent.set_audio_deviation_number, 0, 90, 0.01)
        await new_number(parent, tuner_config, CONF_POWER, parent.set_power_number, 88, 120, 1, mode = number.NumberMode.NUMBER_MODE_SLIDER)
        await new_number(parent, tuner_config, CONF_ANTCAP, parent.set_antcap_number, 0, 191, 1, mode = number.NumberMode.NUMBER_MODE_SLIDER)
    if analog_config := config.get(CONF_SECTION_ANALOG):
        await new_number(parent, analog_config, CONF_LEVEL, parent.set_analog_level_number, 0, 1023, 1, mode = number.NumberMode.NUMBER_MODE_SLIDER)
    if digital_config := config.get(CONF_SECTION_DIGITAL):
        await new_number(parent, digital_config, CONF_SAMPLE_RATE, parent.set_digital_sample_rate_number, 32000, 48000, 1)
    if pilot_config := config.get(CONF_SECTION_PILOT):
        await new_number(parent, pilot_config, CONF_FREQUENCY, parent.set_pilot_frequency_number, 0, 19, 0.001)
        await new_number(parent, pilot_config, CONF_DEVIATION, parent.set_pilot_deviation_number, 0, 90, 0.001)
    if refclk_config := config.get(CONF_SECTION_REFCLK):
        await new_number(parent, refclk_config, CONF_FREQUENCY, parent.set_refclk_frequency_number, 31130, 34406, 1)
        await new_number(parent, refclk_config, CONF_PRESCALER, parent.set_refclk_prescaler_number, 0, 4095, 1)
    if compressor_config := config.get(CONF_SECTION_COMPRESSOR):
        await new_number(parent, compressor_config, CONF_THRESHOLD, parent.set_acomp_threshold_number, -40, 0, 1)
        await new_number(parent, compressor_config, CONF_GAIN, parent.set_acomp_gain_number, 0, 20, 1)
    if limiter_config := config.get(CONF_SECTION_LIMITER):
        await new_number(parent, limiter_config, CONF_RELEASE_TIME, parent.set_limiter_release_time_number, 0.25, 102.4, 0.01, mode = number.NumberMode.NUMBER_MODE_SLIDER)
    if asq_config := config.get(CONF_SECTION_ASQ):
        await new_number(parent, asq_config, CONF_LEVEL_LOW, parent.set_asq_level_low_number, -70, 0, 1)
        await new_number(parent, asq_config, CONF_DURATION_LOW, parent.set_asq_duration_low_number, 0, 65535, 1)
        await new_number(parent, asq_config, CONF_LEVEL_HIGH, parent.set_asq_level_high_number, -70, 0, 1)
        await new_number(parent, asq_config, CONF_DURATION_HIGH, parent.set_asq_duration_high_number, 0, 65535, 1)
    if rds_config := config.get(CONF_SECTION_RDS):
        await new_number(parent, rds_config, CONF_DEVIATION, parent.set_rds_deviation_number, 0, 7.5, 0.01)
