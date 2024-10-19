import esphome.codegen as cg
from esphome.components import number
from esphome.components.number import NumberMode
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_MODE,
    CONF_ANALOG,
    CONF_POWER,
    CONF_LEVEL,
    CONF_THRESHOLD,
    CONF_GAIN,
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
    CONF_TUNER,
    CONF_DIGITAL,
    CONF_PILOT,
    CONF_REFCLK,
    CONF_ACOMP,
    CONF_LIMITER,
    CONF_ASQ,
    CONF_RDS,
    CONF_DEVIATION,
    CONF_ANTCAP,
    CONF_SAMPLE_RATE,
    CONF_PRESCALER,
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
    for_each_conf,
)

TunerFrequencyNumber = si4713_ns.class_("TunerFrequencyNumber", number.Number)
TunerDeviationNumber = si4713_ns.class_("TunerDeviationNumber", number.Number)
TunerPowerNumber = si4713_ns.class_("TunerPowerNumber", number.Number)
TunerAntcapNumber = si4713_ns.class_("TunerAntcapNumber", number.Number)
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
        cv.Optional(CONF_TUNER): cv.Schema(
            {
                cv.Optional(CONF_FREQUENCY): number.number_schema(
                    TunerFrequencyNumber,
                    unit_of_measurement=UNIT_MEGA_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_DEVIATION): number.number_schema(
                    TunerDeviationNumber,
                    unit_of_measurement=UNIT_KILO_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_POWER): number.number_schema(
                    TunerPowerNumber,
                    unit_of_measurement=UNIT_DECIBEL_MICRO_VOLT,
                    device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
                cv.Optional(CONF_ANTCAP): number.number_schema(
                    TunerAntcapNumber,
                    unit_of_measurement=UNIT_PICO_FARAD,
                    device_class=DEVICE_CLASS_EMPTY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            }
        ),
        cv.Optional(CONF_ANALOG): cv.Schema(
            {
                cv.Optional(CONF_LEVEL): number.number_schema(
                    AnalogLevelNumber,
                    unit_of_measurement=UNIT_MILLI_VOLT,
                    device_class=DEVICE_CLASS_VOLTAGE,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            },
        ),
        cv.Optional(CONF_DIGITAL): cv.Schema(
            {
                cv.Optional(CONF_SAMPLE_RATE): number.number_schema(
                    DigitalSampleRateNumber,
                    unit_of_measurement=UNIT_HERTZ,
                    device_class=DEVICE_CLASS_FREQUENCY,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            },
        ),
        cv.Optional(CONF_PILOT): cv.Schema(
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
        cv.Optional(CONF_REFCLK): cv.Schema(
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
        cv.Optional(CONF_ACOMP): cv.Schema(
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
        cv.Optional(CONF_LIMITER): cv.Schema(
            {
                cv.Optional(CONF_RELEASE_TIME): number.number_schema(
                    LimiterReleaseTimeNumber,
                    unit_of_measurement=UNIT_MILLISECOND,
                    device_class=DEVICE_CLASS_DURATION,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                ),
            },
        ),
        cv.Optional(CONF_ASQ): cv.Schema(
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
        cv.Optional(CONF_RDS): cv.Schema(
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


VARIABLES = {
    CONF_TUNER: [
        [CONF_FREQUENCY, 76, 108, 0.05, None],
        [CONF_DEVIATION, 0, 90, 0.01, None],
        [CONF_POWER, 88, 120, 1, NumberMode.NUMBER_MODE_SLIDER],
        [CONF_ANTCAP, 0, 47.75, 0.25, None],
    ],
    CONF_ANALOG: [
        [CONF_LEVEL, 0, 1023, 1, NumberMode.NUMBER_MODE_SLIDER],
    ],
    CONF_DIGITAL: [
        [CONF_SAMPLE_RATE, 32000, 48000, 1, None],
    ],
    CONF_PILOT: [
        [CONF_FREQUENCY, 0, 19, 0.001, None],
        [CONF_DEVIATION, 0, 90, 0.001, None],
    ],
    CONF_REFCLK: [
        [CONF_FREQUENCY, 31130, 34406, 1, None],
        [CONF_PRESCALER, 0, 4095, 1, None],
    ],
    CONF_ACOMP: [
        [CONF_THRESHOLD, -40, 0, 1, None],
        [CONF_GAIN, 0, 20, 1, None],
    ],
    CONF_LIMITER: [
        [CONF_RELEASE_TIME, 0.25, 102.4, 0.01, NumberMode.NUMBER_MODE_SLIDER],
    ],
    CONF_ASQ: [
        [CONF_LEVEL_LOW, -70, 0, 1, None],
        [CONF_DURATION_LOW, 0, 65535, 1, None],
        [CONF_LEVEL_HIGH, -70, 0, 1, None],
        [CONF_DURATION_HIGH, 0, 65535, 1, None],
    ],
    CONF_RDS: [
        [CONF_DEVIATION, 0, 7.5, 0.01, None],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_SI4713_ID])

    async def new_number(c, args, setter):
        # only override mode when it's set to auto in user config
        if CONF_MODE not in c or c[CONF_MODE] == number.NumberMode.NUMBER_MODE_AUTO:
            if args[4] is not None:
                c[CONF_MODE] = args[4]
        n = await number.new_number(
            c, min_value=args[1], max_value=args[2], step=args[3]
        )
        await cg.register_parented(n, parent)
        cg.add(getattr(parent, setter + "_number")(n))

    await for_each_conf(config, VARIABLES, new_number)
