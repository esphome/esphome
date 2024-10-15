import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from .. import (
    CONF_SI4713_ID,
    Si4713Component,
    si4713_ns,
    CONF_,
    ICON_SLEEP,
    ICON_SINE_WAVE,
    ICON_RESISTOR,
#    T1M_SEL,
)

#T1mSelSelect = si4713_ns.class_("T1mSelSelect", select.Select)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_SI4713_ID): cv.use_id(Si4713Component),
#        cv.Optional(CONF_T1M_SEL): select.select_schema(
#            T1mSelSelect,
#            entity_category=ENTITY_CATEGORY_CONFIG,
#            icon=ICON_SLEEP,
#        ),
    }
)


async def new_select(config, id, setter, options):
    if c := config.get(id):
        s = await select.new_select(c, options=list(options.keys()))
        await cg.register_parented(s, config[CONF_SI4713_ID])
        cg.add(setter(s))


async def to_code(config):
    c = await cg.get_variable(config[CONF_SI4713_ID])
#    await new_select(config, CONF_T1M_SEL, c.set_t1m_sel_select, T1M_SEL)
