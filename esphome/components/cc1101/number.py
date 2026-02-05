import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_FREQUENCY

from . import (
    CONF_AGC,
    CONF_CARRIER_SENSE_ABS_THR,
    CONF_CC1101_ID,
    CONF_CHANNEL_SPACING,
    CONF_FILTER_BANDWIDTH,
    CONF_FSK_DEVIATION,
    CONF_IF_FREQUENCY,
    CONF_MSK_DEVIATION,
    CONF_NUM_PREAMBLE,
    CONF_OUTPUT_POWER,
    CONF_PACKET_LENGTH,
    CONF_SYMBOL_RATE,
    CONF_SYNC0,
    CONF_SYNC1,
    CONF_TUNER,
    CC1101Component,
    ns,
)

CC1101Number = ns.class_("CC1101Number", number.Number, cg.PollingComponent)

TYPES_ROOT = {
    CONF_OUTPUT_POWER: {
        "func": "set_output_power",
        "min": -30.0,
        "max": 11.0,
        "step": 0.5,
    },
    CONF_PACKET_LENGTH: {
        "func": "set_packet_length",
        "min": 0,
        "max": 255,
        "step": 1,
    },
}

TYPES_TUNER = {
    CONF_FREQUENCY: {
        "func": "set_frequency",
        "min": 300.0e6,
        "max": 928.0e6,
        "step": 1000.0,  # 1kHz step
    },
    CONF_IF_FREQUENCY: {
        "func": "set_if_frequency",
        "min": 25000,
        "max": 788000,
        "step": 1000.0,
    },
    CONF_FILTER_BANDWIDTH: {
        "func": "set_filter_bandwidth",  # Uses "bandwidth" as key in user example? No, usually specific. User example: "bandwidth"
        "key_override": "bandwidth",
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
}

TYPES_AGC = {
    CONF_CARRIER_SENSE_ABS_THR: {
        "func": "set_carrier_sense_abs_thr",
        "min": -8,
        "max": 7,
        "step": 1,
    },
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
    }
).extend(cv.polling_component_schema("60s"))

# Root
for type, data in TYPES_ROOT.items():
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        {
            cv.Optional(type): number.number_schema(CC1101Number).extend(
                {
                    cv.Optional("min_value", default=data["min"]): cv.float_,
                    cv.Optional("max_value", default=data["max"]): cv.float_,
                    cv.Optional("step", default=data["step"]): cv.float_,
                }
            ),
        }
    )

# Tuner
TUNER_SCHEMA = cv.Schema({})
for type, data in TYPES_TUNER.items():
    key = data.get("key_override", type)
    TUNER_SCHEMA = TUNER_SCHEMA.extend(
        {
            cv.Optional(key): number.number_schema(CC1101Number).extend(
                {
                    cv.Optional("min_value", default=data["min"]): cv.float_,
                    cv.Optional("max_value", default=data["max"]): cv.float_,
                    cv.Optional("step", default=data["step"]): cv.float_,
                }
            ),
        }
    )
CONFIG_SCHEMA = CONFIG_SCHEMA.extend({cv.Optional(CONF_TUNER): TUNER_SCHEMA})

# AGC
AGC_SCHEMA = cv.Schema({})
for type, data in TYPES_AGC.items():
    AGC_SCHEMA = AGC_SCHEMA.extend(
        {
            cv.Optional(type): number.number_schema(CC1101Number).extend(
                {
                    cv.Optional("min_value", default=data["min"]): cv.float_,
                    cv.Optional("max_value", default=data["max"]): cv.float_,
                    cv.Optional("step", default=data["step"]): cv.float_,
                }
            ),
        }
    )
CONFIG_SCHEMA = CONFIG_SCHEMA.extend({cv.Optional(CONF_AGC): AGC_SCHEMA})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    # Root
    for conf_type, conf_data in TYPES_ROOT.items():
        if conf_type in config:
            conf = config[conf_type]
            var = await number.new_number(
                conf,
                min_value=conf["min_value"],
                max_value=conf["max_value"],
                step=conf["step"],
            )
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(
                var.set_type(cg.RawExpression(f"{CC1101Number}::{conf_type.upper()}"))
            )

    # Tuner
    if CONF_TUNER in config:
        tuner_config = config[CONF_TUNER]
        for conf_type, conf_data in TYPES_TUNER.items():
            conf_key = conf_data.get("key_override", conf_type)
            if conf_key in tuner_config:
                conf = tuner_config[conf_key]
                var = await number.new_number(
                    conf,
                    min_value=conf["min_value"],
                    max_value=conf["max_value"],
                    step=conf["step"],
                )
                await cg.register_component(var, conf)
                cg.add(var.set_parent(parent))
                cg.add(
                    var.set_type(
                        cg.RawExpression(f"{CC1101Number}::{conf_type.upper()}")
                    )
                )

    # AGC
    if CONF_AGC in config:
        agc_config = config[CONF_AGC]
        for conf_type, conf_data in TYPES_AGC.items():
            if conf_type in agc_config:
                conf = agc_config[conf_type]
                var = await number.new_number(
                    conf,
                    min_value=conf["min_value"],
                    max_value=conf["max_value"],
                    step=conf["step"],
                )
                await cg.register_component(var, conf)
                cg.add(var.set_parent(parent))
                cg.add(
                    var.set_type(
                        cg.RawExpression(f"{CC1101Number}::{conf_type.upper()}")
                    )
                )
