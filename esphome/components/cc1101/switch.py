import esphome.codegen as cg
from esphome.components import switch
import esphome.config_validation as cv

from . import (
    CONF_CARRIER_SENSE_ABOVE_THRESHOLD,
    CONF_CC1101_ID,
    CONF_CRC_ENABLE,
    CONF_DC_BLOCKING_FILTER,
    CONF_LNA_PRIORITY,
    CONF_MANCHESTER,
    CONF_PACKET_MODE,
    CONF_WHITENING,
    CC1101Component,
    ns,
)

CC1101Switch = ns.class_("CC1101Switch", switch.Switch, cg.PollingComponent)

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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
    }
).extend(cv.polling_component_schema("60s"))

# Root

for type in TYPES_ROOT:
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        {
            cv.Optional(type): switch.switch_schema(CC1101Switch),
        }
    )


# Tuner

for type in TYPES_TUNER:
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        {
            cv.Optional(type): switch.switch_schema(CC1101Switch),
        }
    )


# AGC

for type in TYPES_AGC:
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        {
            cv.Optional(type): switch.switch_schema(CC1101Switch),
        }
    )


async def to_code(config):
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    # Root

    for conf_type in TYPES_ROOT:
        if conf_type in config:
            conf = config[conf_type]

            var = await switch.new_switch(conf)

            await cg.register_component(var, conf)

            cg.add(var.set_parent(parent))

            cg.add(
                var.set_type(cg.RawExpression(f"{CC1101Switch}::{conf_type.upper()}"))
            )

    # Tuner

    for conf_type in TYPES_TUNER:
        if conf_type in config:
            conf = config[conf_type]

            var = await switch.new_switch(conf)

            await cg.register_component(var, conf)

            cg.add(var.set_parent(parent))

            cg.add(
                var.set_type(cg.RawExpression(f"{CC1101Switch}::{conf_type.upper()}"))
            )

    # AGC

    for conf_type in TYPES_AGC:
        if conf_type in config:
            conf = config[conf_type]

            var = await switch.new_switch(conf)

            await cg.register_component(var, conf)

            cg.add(var.set_parent(parent))

            cg.add(
                var.set_type(cg.RawExpression(f"{CC1101Switch}::{conf_type.upper()}"))
            )
