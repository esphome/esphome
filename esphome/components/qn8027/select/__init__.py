import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    CONF_QN8027_ID,
    QN8027Component,
    qn8027_ns,
    CONF_TX_PILOT,
    CONF_T1M_SEL,
    ICON_RADIO_TOWER,
    ICON_SLEEP,
)

TxPilotSelect = qn8027_ns.class_("TxPilotSelect", select.Select)
T1mSelSelect = qn8027_ns.class_("T1mSelSelect", select.Select)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_QN8027_ID): cv.use_id(QN8027Component),
        cv.Optional(CONF_TX_PILOT): select.select_schema(
            TxPilotSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_RADIO_TOWER,
        ),
        cv.Optional(CONF_T1M_SEL): select.select_schema(
            T1mSelSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
    }
)

async def to_code(config):
    qn8027_component = await cg.get_variable(config[CONF_QN8027_ID])
    if tx_pilot_config := config.get(CONF_TX_PILOT):
        s = await select.new_select(tx_pilot_config, options=["7%", "8%", "9%", "10%", "11%", "12%", "13%", "14%", "15%"])
        await cg.register_parented(s, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_tx_pilot_select(s))
    if t1m_sel_config := config.get(CONF_T1M_SEL):
        s = await select.new_select(t1m_sel_config, options=["Never", "58s", "59s", "60s"])
        await cg.register_parented(s, config[CONF_QN8027_ID])
        cg.add(qn8027_component.set_t1m_sel_select(s))