import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import select
from esphome.const import CONF_ID
from . import (
    CC1101Component, CONF_RX_ATTENUATION, CONF_SYNC_MODE, CONF_MODULATION_TYPE,
    CONF_MAGN_TARGET, CONF_MAX_LNA_GAIN, CONF_MAX_DVGA_GAIN, CONF_CARRIER_SENSE_REL_THR,
    CONF_FILTER_LENGTH_FSK_MSK, CONF_FILTER_LENGTH_ASK_OOK, CONF_FREEZE, CONF_WAIT_TIME,
    CONF_HYST_LEVEL, RX_ATTENUATION, SYNC_MODE, MODULATION, MAGN_TARGET, MAX_LNA_GAIN,
    MAX_DVGA_GAIN, CARRIER_SENSE_REL_THR, FILTER_LENGTH_FSK_MSK, FILTER_LENGTH_ASK_OOK,
    FREEZE, WAIT_TIME, HYST_LEVEL, ns
)

CC1101Select = ns.class_("CC1101Select", select.Select, cg.PollingComponent)

CONF_CC1101_ID = "cc1101_id"
CONF_FREQUENCY_PRESET = "frequency_preset"

TYPES = {
    CONF_RX_ATTENUATION: RX_ATTENUATION,
    CONF_SYNC_MODE: SYNC_MODE,
    CONF_MODULATION_TYPE: MODULATION,
    CONF_MAGN_TARGET: MAGN_TARGET,
    CONF_MAX_LNA_GAIN: MAX_LNA_GAIN,
    CONF_MAX_DVGA_GAIN: MAX_DVGA_GAIN,
    CONF_CARRIER_SENSE_REL_THR: CARRIER_SENSE_REL_THR,
    CONF_FILTER_LENGTH_FSK_MSK: FILTER_LENGTH_FSK_MSK,
    CONF_FILTER_LENGTH_ASK_OOK: FILTER_LENGTH_ASK_OOK,
    CONF_FREEZE: FREEZE,
    CONF_WAIT_TIME: WAIT_TIME,
    CONF_HYST_LEVEL: HYST_LEVEL,
}

FREQUENCY_PRESETS = ["315MHz", "433.92MHz", "868MHz", "915MHz"]

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
    cv.Optional(CONF_FREQUENCY_PRESET): select.select_schema(CC1101Select),
}).extend(cv.polling_component_schema("60s"))

for type, options in TYPES.items():
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend({
        cv.Optional(type): select.select_schema(CC1101Select),
    })

async def to_code(config):
    cg.add(cg.include("esphome/components/cc1101/cc1101_select.h"))
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    if CONF_FREQUENCY_PRESET in config:
        conf = config[CONF_FREQUENCY_PRESET]
        var = await select.new_select(conf, options=FREQUENCY_PRESETS)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_type(getattr(CC1101Select, "FREQUENCY_PRESET")))

    for type, options in TYPES.items():
        if type in config:
            conf = config[type]
            # keys are strings
            opts = list(options.keys())
            var = await select.new_select(conf, options=opts)
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(var.set_type(getattr(CC1101Select, type.upper())))
