import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID
from . import CC1101Component, CONF_DC_BLOCKING_FILTER, CONF_CARRIER_SENSE_ABOVE_THRESHOLD, \
    CONF_MANCHESTER, CONF_LNA_PRIORITY, CONF_PACKET_MODE, CONF_CRC_ENABLE, CONF_WHITENING, ns

CC1101Switch = ns.class_("CC1101Switch", switch.Switch, cg.PollingComponent)

CONF_CC1101_ID = "cc1101_id"

TYPES = {
    CONF_DC_BLOCKING_FILTER: "set_dc_blocking_filter",
    CONF_CARRIER_SENSE_ABOVE_THRESHOLD: "set_carrier_sense_above_threshold",
    CONF_MANCHESTER: "set_manchester",
    CONF_LNA_PRIORITY: "set_lna_priority",
    CONF_PACKET_MODE: "set_packet_mode",
    CONF_CRC_ENABLE: "set_crc_enable",
    CONF_WHITENING: "set_whitening",
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
}).extend(cv.polling_component_schema("60s"))

for type, _ in TYPES.items():
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend({
        cv.Optional(type): switch.switch_schema(CC1101Switch),
    })

async def to_code(config):
    cg.add(cg.include("esphome/components/cc1101/cc1101_switch.h"))
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    for type, func_name in TYPES.items():
        if type in config:
            conf = config[type]
            var = await switch.new_switch(conf)
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(var.set_type(getattr(CC1101Switch, type.upper())))
