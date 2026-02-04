import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from . import CC1101Component, CONF_DC_BLOCKING_FILTER, CONF_CARRIER_SENSE_ABOVE_THRESHOLD, \
    CONF_MANCHESTER, CONF_LNA_PRIORITY, CONF_PACKET_MODE, CONF_CRC_ENABLE, CONF_WHITENING, ns

CC1101Switch = ns.class_("CC1101Switch", switch.Switch, cg.PollingComponent)

CONF_CC1101_ID = "cc1101_id"
CONF_TUNER = "tuner"
CONF_AGC = "agc"

TYPES_ROOT = {
    CONF_DC_BLOCKING_FILTER: "set_dc_blocking_filter",
    CONF_PACKET_MODE: "set_packet_mode",
    CONF_CRC_ENABLE: "set_crc_enable",
    CONF_WHITENING: "set_whitening",
}

TYPES_TUNER = {
    CONF_CARRIER_SENSE_ABOVE_THRESHOLD: "set_carrier_sense_above_threshold",
    CONF_MANCHESTER: "set_manchester",
}

TYPES_AGC = {
    CONF_LNA_PRIORITY: "set_lna_priority",
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
}).extend(cv.polling_component_schema("60s"))

# Root
for type, _ in TYPES_ROOT.items():
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend({
        cv.Optional(type): switch.switch_schema(CC1101Switch),
    })

# Tuner
TUNER_SCHEMA = cv.Schema({})
for type, _ in TYPES_TUNER.items():
    TUNER_SCHEMA = TUNER_SCHEMA.extend({
        cv.Optional(type): switch.switch_schema(CC1101Switch),
    })
CONFIG_SCHEMA = CONFIG_SCHEMA.extend({cv.Optional(CONF_TUNER): TUNER_SCHEMA})

# AGC
AGC_SCHEMA = cv.Schema({})
for type, _ in TYPES_AGC.items():
    AGC_SCHEMA = AGC_SCHEMA.extend({
        cv.Optional(type): switch.switch_schema(CC1101Switch),
    })
CONFIG_SCHEMA = CONFIG_SCHEMA.extend({cv.Optional(CONF_AGC): AGC_SCHEMA})

async def to_code(config):
    cg.add(cg.include("esphome/components/cc1101/cc1101_switch.h"))
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    # Root
    for type, _ in TYPES_ROOT.items():
        if type in config:
            conf = config[type]
            var = await switch.new_switch(conf)
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(var.set_type(getattr(CC1101Switch, type.upper())))

    # Tuner
    if CONF_TUNER in config:
        tuner_config = config[CONF_TUNER]
        for type, _ in TYPES_TUNER.items():
            if type in tuner_config:
                conf = tuner_config[type]
                var = await switch.new_switch(conf)
                await cg.register_component(var, conf)
                cg.add(var.set_parent(parent))
                cg.add(var.set_type(getattr(CC1101Switch, type.upper())))

    # AGC
    if CONF_AGC in config:
        agc_config = config[CONF_AGC]
        for type, _ in TYPES_AGC.items():
            if type in agc_config:
                conf = agc_config[type]
                var = await switch.new_switch(conf)
                await cg.register_component(var, conf)
                cg.add(var.set_parent(parent))
                cg.add(var.set_type(getattr(CC1101Switch, type.upper())))
