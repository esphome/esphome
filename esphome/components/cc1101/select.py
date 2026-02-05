import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv

from . import (
    CARRIER_SENSE_REL_THR,
    CONF_AGC,
    CONF_CARRIER_SENSE_REL_THR,
    CONF_CC1101_ID,
    CONF_FILTER_LENGTH_ASK_OOK,
    CONF_FILTER_LENGTH_FSK_MSK,
    CONF_FREEZE,
    CONF_HYST_LEVEL,
    CONF_MAGN_TARGET,
    CONF_MAX_DVGA_GAIN,
    CONF_MAX_LNA_GAIN,
    CONF_MODULATION_TYPE,
    CONF_RX_ATTENUATION,
    CONF_SYNC_MODE,
    CONF_TUNER,
    CONF_WAIT_TIME,
    FILTER_LENGTH_ASK_OOK,
    FILTER_LENGTH_FSK_MSK,
    FREEZE,
    HYST_LEVEL,
    MAGN_TARGET,
    MAX_DVGA_GAIN,
    MAX_LNA_GAIN,
    MODULATION,
    RX_ATTENUATION,
    SYNC_MODE,
    WAIT_TIME,
    CC1101Component,
    ns,
)

CC1101Select = ns.class_("CC1101Select", select.Select, cg.PollingComponent)

CONF_FREQUENCY_PRESET = "frequency_preset"

FREQUENCY_PRESETS = ["315MHz", "433.92MHz", "868MHz", "915MHz"]

TYPES_ROOT = {
    CONF_RX_ATTENUATION: RX_ATTENUATION,
}

TYPES_TUNER = {
    CONF_SYNC_MODE: SYNC_MODE,
    CONF_MODULATION_TYPE: {"key_override": "modulation", "options": MODULATION},
}

TYPES_AGC = {
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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CC1101_ID): cv.use_id(CC1101Component),
        cv.Optional(CONF_FREQUENCY_PRESET): select.select_schema(CC1101Select),
    }
).extend(cv.polling_component_schema("60s"))

# Root

for type in TYPES_ROOT:
    CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
        {
            cv.Optional(type): select.select_schema(CC1101Select),
        }
    )


# Tuner

TUNER_SCHEMA = cv.Schema({})

for type, data in TYPES_TUNER.items():
    if isinstance(data, dict) and "key_override" in data:
        key = data["key_override"]

    else:
        key = type

    TUNER_SCHEMA = TUNER_SCHEMA.extend(
        {
            cv.Optional(key): select.select_schema(CC1101Select),
        }
    )

CONFIG_SCHEMA = CONFIG_SCHEMA.extend({cv.Optional(CONF_TUNER): TUNER_SCHEMA})


# AGC

AGC_SCHEMA = cv.Schema({})

for type in TYPES_AGC:
    AGC_SCHEMA = AGC_SCHEMA.extend(
        {
            cv.Optional(type): select.select_schema(CC1101Select),
        }
    )

CONFIG_SCHEMA = CONFIG_SCHEMA.extend({cv.Optional(CONF_AGC): AGC_SCHEMA})


async def to_code(config):
    parent = await cg.get_variable(config[CONF_CC1101_ID])

    if CONF_FREQUENCY_PRESET in config:
        conf = config[CONF_FREQUENCY_PRESET]
        var = await select.new_select(conf, options=FREQUENCY_PRESETS)
        await cg.register_component(var, conf)
        cg.add(var.set_parent(parent))
        cg.add(var.set_type(cg.RawExpression(f"{CC1101Select}::FREQUENCY_PRESET")))

    # Root
    for conf_type, conf_options in TYPES_ROOT.items():
        if conf_type in config:
            conf = config[conf_type]
            opts = list(conf_options.keys())
            var = await select.new_select(conf, options=opts)
            await cg.register_component(var, conf)
            cg.add(var.set_parent(parent))
            cg.add(
                var.set_type(cg.RawExpression(f"{CC1101Select}::{conf_type.upper()}"))
            )

    # Tuner
    if CONF_TUNER in config:
        tuner_config = config[CONF_TUNER]
        for conf_type, conf_data in TYPES_TUNER.items():
            if isinstance(conf_data, dict) and "key_override" in conf_data:
                conf_key = conf_data["key_override"]
                conf_options = conf_data["options"]
            else:
                conf_key = conf_type
                conf_options = conf_data

            if conf_key in tuner_config:
                conf = tuner_config[conf_key]
                opts = list(conf_options.keys())
                var = await select.new_select(conf, options=opts)
                await cg.register_component(var, conf)
                cg.add(var.set_parent(parent))
                cg.add(
                    var.set_type(
                        cg.RawExpression(f"{CC1101Select}::{conf_type.upper()}")
                    )
                )

    # AGC
    if CONF_AGC in config:
        agc_config = config[CONF_AGC]
        for conf_type, conf_options in TYPES_AGC.items():
            if conf_type in agc_config:
                conf = agc_config[conf_type]
                opts = list(conf_options.keys())
                var = await select.new_select(conf, options=opts)
                await cg.register_component(var, conf)
                cg.add(var.set_parent(parent))
                cg.add(
                    var.set_type(
                        cg.RawExpression(f"{CC1101Select}::{conf_type.upper()}")
                    )
                )
