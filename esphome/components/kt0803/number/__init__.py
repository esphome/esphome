import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_GAIN,
    CONF_MODE,
    UNIT_DECIBEL,
    DEVICE_CLASS_FREQUENCY,
    DEVICE_CLASS_SIGNAL_STRENGTH,
    ENTITY_CATEGORY_CONFIG,
)
from .. import (
    CONF_KT0803_ID,
    KT0803Component,
    kt0803_ns,
    CONF_ALC,
    CONF_PGA,
    CONF_RFGAIN,
    UNIT_MEGA_HERTZ,
    UNIT_DECIBEL_MICRO_VOLT,
    for_each_conf,
)

FrequencyNumber = kt0803_ns.class_("FrequencyNumber", number.Number)
PgaNumber = kt0803_ns.class_("PgaNumber", number.Number)
RfGainNumber = kt0803_ns.class_("RfGainNumber", number.Number)
AlcGainNumber = kt0803_ns.class_("AlcGainNumber", number.Number)

ALC_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_GAIN): number.number_schema(
            AlcGainNumber,
            unit_of_measurement=UNIT_DECIBEL,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_KT0803_ID): cv.use_id(KT0803Component),
        cv.Optional(CONF_FREQUENCY): number.number_schema(
            FrequencyNumber,
            unit_of_measurement=UNIT_MEGA_HERTZ,
            device_class=DEVICE_CLASS_FREQUENCY,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_PGA): number.number_schema(
            PgaNumber,
            unit_of_measurement=UNIT_DECIBEL,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_RFGAIN): number.number_schema(
            RfGainNumber,
            unit_of_measurement=UNIT_DECIBEL_MICRO_VOLT,
            device_class=DEVICE_CLASS_SIGNAL_STRENGTH,
            entity_category=ENTITY_CATEGORY_CONFIG,
        ),
        cv.Optional(CONF_ALC): ALC_SCHEMA,
    }
)

VARIABLES = {
    None: [
        [
            CONF_FREQUENCY,
            kt0803_ns.CHSEL_MIN,
            kt0803_ns.CHSEL_MAX,
            kt0803_ns.CHSEL_STEP,
            None,
        ],
        [CONF_PGA, kt0803_ns.PGA_MIN, kt0803_ns.PGA_MAX, kt0803_ns.PGA_STEP, None],
        [CONF_RFGAIN, kt0803_ns.RFGAIN_MIN, kt0803_ns.RFGAIN_MAX, 0.1, None],
    ],
    CONF_ALC: [
        [CONF_GAIN, kt0803_ns.ALC_GAIN_MIN, kt0803_ns.ALC_GAIN_MAX, 3, None],
    ],
}


async def to_code(config):
    parent = await cg.get_variable(config[CONF_KT0803_ID])

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
