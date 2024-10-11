import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
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
    CONF_FREQUENCY_DEVIATION,
    CONF_TX_PILOT,
    CONF_XTAL_CURRENT,
    CONF_INPUT_GAIN,
    CONF_DIGITAL_GAIN,
    CONF_POWER_TARGET,
    CONF_RDS_FREQUENCY_DEVIATION,
    UNIT_MEGA_HERTZ,
    UNIT_KILO_HERTZ,
    UNIT_MICRO_AMPERE,
    UNIT_DECIBEL_MICRO_VOLT,
)

FrequencyNumber = qn8027_ns.class_("FrequencyNumber", number.Number)
FrequencyDeviationNumber = qn8027_ns.class_("FrequencyDeviationNumber", number.Number)
TxPilotNumber = qn8027_ns.class_("TxPilotNumber", number.Number)
XtalCurrentNumber = qn8027_ns.class_("XtalCurrentNumber", number.Number)
InputGainNumber = qn8027_ns.class_("InputGainNumber", number.Number)
DigitalGainNumber = qn8027_ns.class_("DigitalGainNumber", number.Number)
PowerTargetNumber = qn8027_ns.class_("PowerTargetNumber", number.Number)
RDSFrequencyDeviationNumber = qn8027_ns.class_(
    "RDSFrequencyDeviationNumber", number.Number
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
        cv.Optional(CONF_FREQUENCY_DEVIATION): number.number_schema(
            FrequencyDeviationNumber,
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
        cv.Optional(CONF_XTAL_CURRENT): number.number_schema(
            XtalCurrentNumber,
            unit_of_measurement=UNIT_MICRO_AMPERE,
            device_class=DEVICE_CLASS_CURRENT,
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
        cv.Optional(CONF_RDS_FREQUENCY_DEVIATION): number.number_schema(
            RDSFrequencyDeviationNumber,
            unit_of_measurement=UNIT_KILO_HERTZ,
            device_class=DEVICE_CLASS_FREQUENCY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)


async def new_number(config, id, setter, min_value, max_value, step, *args):
    if c := config.get(id):
        n = await number.new_number(
            c, *args, min_value=min_value, max_value=max_value, step=step
        )
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(setter(n))


async def to_code(config):
    c = await cg.get_variable(config[CONF_QN8027_ID])
    await new_number(config, CONF_FREQUENCY, c.set_frequency_number, 76, 108, 0.05)
    await new_number(
        config,
        CONF_FREQUENCY_DEVIATION,
        c.set_frequency_deviation_number,
        0,
        147.9,
        0.58,
   	)
    await new_number(config, CONF_TX_PILOT, c.set_tx_pilot_number, 7, 15, 1)
    await new_number(config, CONF_XTAL_CURRENT, c.set_xtal_current_number, 0, 393.75, 6.25)
    await new_number(config, CONF_INPUT_GAIN, c.set_input_gain_number, 0, 5, 1)
    await new_number(config, CONF_DIGITAL_GAIN, c.set_digital_gain_number, 0, 2, 1)
    await new_number(
        config, CONF_POWER_TARGET, c.set_power_target_number, 83.4, 117.5, 0.62
    )
    await new_number(
        config,
        CONF_RDS_FREQUENCY_DEVIATION,
        c.set_rds_frequency_deviation_number,
        0,
        44.45,
        0.35,
    )
