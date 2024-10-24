import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_CURRENT,
    CONF_MODE,
    UNIT_PERCENT,
    UNIT_DECIBEL,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    DEVICE_CLASS_EMPTY,
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    CONF_QN8027_ID,
    QN8027Component,
    qn8027_ns,
    CONF_XTAL,
    CONF_RDS,
    CONF_DEVIATION,
    CONF_TX_PILOT,
    CONF_INPUT_GAIN,
    CONF_DIGITAL_GAIN,
    CONF_POWER_TARGET,
    UNIT_MEGA_HERTZ,
    UNIT_KILO_HERTZ,
    UNIT_MICRO_AMPERE,
    UNIT_DECIBEL_MICRO_VOLT,
    for_each_conf,
)

FrequencyNumber = qn8027_ns.class_("FrequencyNumber", number.Number)
DeviationNumber = qn8027_ns.class_("DeviationNumber", number.Number)
TxPilotNumber = qn8027_ns.class_("TxPilotNumber", number.Number)
InputGainNumber = qn8027_ns.class_("InputGainNumber", number.Number)
DigitalGainNumber = qn8027_ns.class_("DigitalGainNumber", number.Number)
PowerTargetNumber = qn8027_ns.class_("PowerTargetNumber", number.Number)
XtalCurrentNumber = qn8027_ns.class_("XtalCurrentNumber", number.Number)
RDSDeviationNumber = qn8027_ns.class_("RDSDeviationNumber", number.Number)

XTAL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_CURRENT): number.number_schema(
            XtalCurrentNumber,
            unit_of_measurement=UNIT_MICRO_AMPERE,
            device_class=DEVICE_CLASS_CURRENT,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)

RDS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DEVIATION): number.number_schema(
            RDSDeviationNumber,
            unit_of_measurement=UNIT_KILO_HERTZ,
            device_class=DEVICE_CLASS_FREQUENCY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_QN8027_ID): cv.use_id(QN8027Component),
        cv.Optional(CONF_FREQUENCY): number.number_schema(
            FrequencyNumber,
            unit_of_measurement=UNIT_MEGA_HERTZ,
            device_class=DEVICE_CLASS_FREQUENCY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_DEVIATION): number.number_schema(
            DeviationNumber,
            unit_of_measurement=UNIT_KILO_HERTZ,
            device_class=DEVICE_CLASS_FREQUENCY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_TX_PILOT): number.number_schema(
            TxPilotNumber,
            unit_of_measurement=UNIT_PERCENT,
            device_class=DEVICE_CLASS_EMPTY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_INPUT_GAIN): number.number_schema(
            InputGainNumber,
            unit_of_measurement=UNIT_DECIBEL,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_DIGITAL_GAIN): number.number_schema(
            DigitalGainNumber,
            unit_of_measurement=UNIT_DECIBEL,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_POWER_TARGET): number.number_schema(
            PowerTargetNumber,
            unit_of_measurement=UNIT_DECIBEL_MICRO_VOLT,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_XTAL): XTAL_SCHEMA,
        cv.Optional(CONF_RDS): RDS_SCHEMA,
    }
)

VARIABLES = {
    None: [
        [CONF_FREQUENCY, 76, 108, 0.05, None],
        [CONF_DEVIATION, 0, 147.9, 0.58, None],
        [CONF_TX_PILOT, 7, 15, 1, None],
        [CONF_INPUT_GAIN, 0, 5, 1, None],
        [CONF_DIGITAL_GAIN, 0, 2, 1, None],
        [CONF_POWER_TARGET, 83.4, 117.5, 0.62, None],
    ],
    CONF_XTAL: [
        [CONF_CURRENT, 0, 393.75, 6.25, None],
    ],
    CONF_RDS: [
        [CONF_DEVIATION, 0, 44.45, 0.35, None],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_QN8027_ID])

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
