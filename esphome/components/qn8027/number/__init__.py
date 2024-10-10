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


async def to_code(config):
    qn8027_component = await cg.get_variable(config[CONF_QN8027_ID])
    if frequency_config := config.get(CONF_FREQUENCY):
        n = await number.new_number(
            frequency_config, min_value=76, max_value=108, step=0.05
        )
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_frequency_number(n))
    if frequency_deviation_config := config.get(CONF_FREQUENCY_DEVIATION):
        n = await number.new_number(
            frequency_deviation_config, min_value=0, max_value=147.9, step=0.58
        )
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_frequency_deviation_number(n))
    if tx_pilot_config := config.get(CONF_TX_PILOT):
        n = await number.new_number(tx_pilot_config, min_value=7, max_value=15, step=1)
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_tx_pilot_number(n))
    if xtal_current_config := config.get(CONF_XTAL_CURRENT):
        n = await number.new_number(
            xtal_current_config, min_value=0, max_value=393.75, step=6.25
        )
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_xtal_current_number(n))
    if input_gain_config := config.get(CONF_INPUT_GAIN):
        n = await number.new_number(input_gain_config, min_value=0, max_value=5, step=1)
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_input_gain_number(n))
    if digital_gain_config := config.get(CONF_DIGITAL_GAIN):
        n = await number.new_number(
            digital_gain_config, min_value=0, max_value=2, step=1
        )
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_digital_gain_number(n))
    if power_target_config := config.get(CONF_POWER_TARGET):
        n = await number.new_number(
            power_target_config, min_value=83.4, max_value=117.5, step=0.62
        )
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_power_target_number(n))
    if rds_frequency_deviation_config := config.get(CONF_RDS_FREQUENCY_DEVIATION):
        n = await number.new_number(
            rds_frequency_deviation_config, min_value=0, max_value=44.45, step=0.35
        )
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_rds_frequency_deviation_number(n))
