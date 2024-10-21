import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    ENTITY_CATEGORY_CONFIG,
    ICON_PULSE,
)
from .. import (
    CONF_QN8027_ID,
    QN8027Component,
    qn8027_ns,
    CONF_XTAL,
    CONF_T1M_SEL,
    CONF_PRE_EMPHASIS,
    CONF_INPUT_IMPEDANCE,
    CONF_SOURCE,
    ICON_SLEEP,
    ICON_SINE_WAVE,
    ICON_RESISTOR,
    T1M_SEL,
    PRE_EMPHASIS,
    XTAL_SOURCE,
    XTAL_FREQUENCY,
    INPUT_IMPEDANCE,
    for_each_conf,
)

T1mSelSelect = qn8027_ns.class_("T1mSelSelect", select.Select)
PreEmphasisSelect = qn8027_ns.class_("PreEmphasisSelect", select.Select)
InputImpedanceSelect = qn8027_ns.class_("InputImpedanceSelect", select.Select)
XtalSourceSelect = qn8027_ns.class_("XtalSourceSelect", select.Select)
XtalFrequencySelect = qn8027_ns.class_("XtalFrequencySelect", select.Select)

XTAL_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_SOURCE): select.select_schema(
            XtalSourceSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_PULSE,
        ),
        cv.Optional(CONF_FREQUENCY): select.select_schema(
            XtalFrequencySelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_SINE_WAVE,
        ),
    }
)

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
        cv.Optional(CONF_INPUT_IMPEDANCE): select.select_schema(
            InputImpedanceSelect,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_RESISTOR,
        ),
        cv.Optional(CONF_XTAL): XTAL_SCHEMA,
    }
)


VARIABLES = {
    None: [
        [CONF_T1M_SEL, T1M_SEL],
        [CONF_PRE_EMPHASIS, PRE_EMPHASIS],
        [CONF_INPUT_IMPEDANCE, INPUT_IMPEDANCE],
    ],
    CONF_XTAL: [
        [CONF_SOURCE, XTAL_SOURCE],
        [CONF_FREQUENCY, XTAL_FREQUENCY],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_QN8027_ID])

    async def new_select(c, args, setter):
        s = await select.new_select(c, options=list(args[1].keys()))
        await cg.register_parented(s, parent)
        cg.add(getattr(parent, setter + "_select")(s))

    await for_each_conf(config, VARIABLES, new_select)
