import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    CONF_ID, CONF_FREQUENCY, CONF_CHANNEL, CONF_OUTPUT_POWER, CONF_NUM_PREAMBLE,
    CONF_SYNC1, CONF_SYNC0
)
from . import (
    CC1101Component, CONF_IF_FREQUENCY, CONF_FILTER_BANDWIDTH, CONF_CHANNEL_SPACING,
    CONF_FSK_DEVIATION, CONF_MSK_DEVIATION, CONF_SYMBOL_RATE, CONF_CARRIER_SENSE_ABS_THR,
    CONF_PACKET_LENGTH, ns
)

CC1101Number = ns.class_("CC1101Number", number.Number, cg.PollingComponent)

CONF_CC1101_ID = "cc1101_id"

TYPES = {
    CONF_OUTPUT_POWER: {
        "func": "set_output_power",
        "min": -30.0,
        "max": 11.0,
        "step": 0.5,
    },
    CONF_FREQUENCY: {
        "func": "set_frequency",
        "min": 300.0e6,
        "max": 928.0e6,
        "step": 1000.0, # 1kHz step
    },
    CONF_IF_FREQUENCY: {
        "func": "set_if_frequency",
        "min": 25000,
        "max": 788000,
        "step": 1000.0,
    },
    CONF_FILTER_BANDWIDTH: {
        "func": "set_filter_bandwidth",
        "min": 58000,
        "max": 812000,
        "step": 1000.0,
    },
    CONF_CHANNEL: {
        "func": "set_channel",
        "min": 0,
        "max": 255,
        "step": 1,
    },
    CONF_CHANNEL_SPACING: {
        "func": "set_channel_spacing",
        "min": 25000,
        "max": 405000,
        "step": 1000.0,
    },
    CONF_FSK_DEVIATION: {
        "func": "set_fsk_deviation",
        "min": 1500,
        "max": 381000,
        "step": 100.0,
    },
    CONF_MSK_DEVIATION: {
        "func": "set_msk_deviation",
        "min": 1,
        "max": 8,
        "step": 1,
    },
    CONF_SYMBOL_RATE: {
        "func": "set_symbol_rate",
        "min": 600,
        "max": 500000,
        "step": 100.0,
    },
    CONF_NUM_PREAMBLE: {
        "func": "set_num_preamble",
        "min": 0,
        "max": 7,
        "step": 1,
    },
    CONF_SYNC1: {
        "func": "set_sync1",
        "min": 0,
        "max": 255,
        "step": 1,
    },
    CONF_SYNC0: {
        "func": "set_sync0",
        "min": 0,
        "max": 255,
        "step": 1,
    },
    CONF_CARRIER_SENSE_ABS_THR: {
        "func": "set_carrier_sense_abs_thr",
        "min": -8,
        "max": 7,
        "step": 1,
    },
    CONF_PACKET_LENGTH: {
        "func": "set_packet_length",
        "min": 0,
        "max": 255,
        "step": 1,
    },
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
}).extend(cv.polling_component_schema("60s"))

for type, data in TYPES.items():
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend({
        cv.Optional(type): number.number_schema(CC1101Number).extend({
            cv.Optional("min_value", default=data["min"]): cv.float_,
            cv.Optional("max_value", default=data["max"]): cv.float_,
            cv.Optional("step", default=data["step"]): cv.float_,
        }),
    })

async def to_code(config):
    cg.add(cg.include("esphome/components/cc1101/cc1101_number.h"))
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    for type, data in TYPES.items():
        if type in config:
            conf = config[type]
            var = await number.new_number(conf, min_value=conf["min_value"], max_value=conf["max_value"], step=conf["step"])
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(var.set_type(getattr(CC1101Number, type.upper())))
