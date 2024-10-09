import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    UNIT_PERCENT,
    UNIT_SECOND,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_DURATION,
    DEVICE_CLASS_EMPTY,
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    CONF_QN8027_ID,
    QN8027Component,
    qn8027_ns,
    CONF_FREQUENCY_DEVIATION,
    CONF_TX_PILOT,
    CONF_T1M_SEL,
    UNIT_MEGA_HERTZ,
    UNIT_KILO_HERTZ,
)

FrequencyNumber = qn8027_ns.class_("FrequencyNumber", number.Number)
FrequencyDeviationNumber = qn8027_ns.class_("FrequencyDeviationNumber", number.Number)
TxPilotNumber = qn8027_ns.class_("TxPilotNumber", number.Number)
T1mSelNumber = qn8027_ns.class_("T1mSelNumber", number.Number)

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
        cv.Optional(CONF_T1M_SEL): number.number_schema(
            T1mSelNumber,
            unit_of_measurement=UNIT_SECOND,
            device_class=DEVICE_CLASS_DURATION,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)

async def to_code(config):
    qn8027_component = await cg.get_variable(config[CONF_QN8027_ID])
    if frequency_config := config.get(CONF_FREQUENCY):
        n = await number.new_number(frequency_config, min_value=76, max_value=108, step=0.05)
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_frequency_number(n))
    if frequency_deviation_config := config.get(CONF_FREQUENCY_DEVIATION):
        n = await number.new_number(frequency_deviation_config, min_value=0, max_value=147.9, step=0.58)
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_frequency_deviation_number(n))
    if tx_pilot_config := config.get(CONF_TX_PILOT):
        n = await number.new_number(tx_pilot_config, min_value=7, max_value=15, step=1)
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_tx_pilot_number(n))
    if t1m_sel_config := config.get(CONF_T1M_SEL):
        n = await number.new_number(t1m_sel_config, min_value=58, max_value=60, step=1)
        await cg.register_parented(n, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_t1m_sel_number(n))