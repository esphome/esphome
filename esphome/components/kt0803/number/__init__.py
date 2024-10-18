import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_GAIN,
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


async def new_number(p, config, id, setter, min_value, max_value, step):
    if c := config.get(id):
        n = await number.new_number(
            c, min_value=min_value, max_value=max_value, step=step
        )
        await cg.register_parented(n, p)
        cg.add(setter(n))
        return n


async def to_code(config):
    p = await cg.get_variable(config[CONF_KT0803_ID])
    await new_number(
        p,
        config,
        CONF_FREQUENCY,
        p.set_frequency_number,
        kt0803_ns.CHSEL_MIN,
        kt0803_ns.CHSEL_MAX,
        kt0803_ns.CHSEL_STEP,
    )
    await new_number(
        p,
        config,
        CONF_PGA,
        p.set_pga_number,
        kt0803_ns.PGA_MIN,
        kt0803_ns.PGA_MAX,
        kt0803_ns.PGA_STEP,
    )
    await new_number(
        p,
        config,
        CONF_RFGAIN,
        p.set_rfgain_number,
        kt0803_ns.RFGAIN_MIN,
        kt0803_ns.RFGAIN_MAX,
        0.1,
    )
    if alc_config := config.get(CONF_ALC):
        await new_number(
            p,
            alc_config,
            CONF_GAIN,
            p.set_alc_gain_number,
            kt0803_ns.ALC_GAIN_MIN,
            kt0803_ns.ALC_GAIN_MAX,
            3,
        )
