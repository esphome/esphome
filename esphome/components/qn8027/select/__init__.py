import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from .. import (
    CONF_QN8027_ID,
    QN8027Component,
    qn8027_ns,
    CONF_T1M_SEL,
    CONF_PRE_EMPHASIS,
    CONF_XTAL_SOURCE,
    CONF_XTAL_FREQUENCY,
    CONF_INPUT_IMPEDANCE,
    ICON_SLEEP,
    ICON_SINE_WAVE,
    ICON_RESISTOR,
    T1M_SEL,
    PRE_EMPHASIS,
    XTAL_SOURCE,
    XTAL_FREQUENCY,
    INPUT_IMPEDANCE,
)

T1mSelSelect = qn8027_ns.class_("T1mSelSelect", select.Select)
PreEmphasisSelect = qn8027_ns.class_("PreEmphasisSelect", select.Select)
XtalSourceSelect = qn8027_ns.class_("XtalSourceSelect", select.Select)
XtalFrequencySelect = qn8027_ns.class_("XtalFrequencySelect", select.Select)
InputImpedanceSelect = qn8027_ns.class_("InputImpedanceSelect", select.Select)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_QN8027_ID): cv.use_id(QN8027Component),
        cv.Optional(CONF_T1M_SEL): select.select_schema(
            T1mSelSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SLEEP,
        ),
        cv.Optional(CONF_PRE_EMPHASIS): select.select_schema(
            PreEmphasisSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_XTAL_SOURCE): select.select_schema(
            XtalSourceSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_PULSE,
        ),
        cv.Optional(CONF_XTAL_FREQUENCY): select.select_schema(
            XtalFrequencySelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
        cv.Optional(CONF_INPUT_IMPEDANCE): select.select_schema(
            InputImpedanceSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_RESISTOR,
        ),
    }
)


async def new_select(config, id, setter, options):
    if c := config.get(id):
        s = await select.new_select(c, options=list(options.keys()))
        await cg.register_parented(s, config[CONF_QN8027_ID])
        cg.add(setter(s))


async def to_code(config):
    c = await cg.get_variable(config[CONF_QN8027_ID])
    await new_select(config, CONF_T1M_SEL, c.set_t1m_sel_select, T1M_SEL)
    await new_select(config, CONF_PRE_EMPHASIS, c.set_pre_emphasis_select, PRE_EMPHASIS)
    await new_select(config, CONF_XTAL_SOURCE, c.set_xtal_source_select, XTAL_SOURCE)
    await new_select(
    	config, CONF_XTAL_FREQUENCY, c.set_xtal_frequency_select, XTAL_FREQUENCY
    )
    await new_select(
    	config, CONF_INPUT_IMPEDANCE, c.set_input_impedance_select, INPUT_IMPEDANCE
    )
