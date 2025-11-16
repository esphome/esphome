from esphome import automation, pins
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import remote_base, spi
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_FREQUENCY, CONF_ID, CONF_WAIT_TIME

from .const import (
    CARRIER_SENSE_REL_THR,
    CONF_CARRIER_SENSE_ABOVE_THRESHOLD,
    CONF_CARRIER_SENSE_ABS_THR,
    CONF_CARRIER_SENSE_REL_THR,
    CONF_CHANNEL_SPACING,
    CONF_DC_BLOCKING_FILTER,
    CONF_FILTER_BANDWIDTH,
    CONF_FILTER_LENGTH_ASK_OOK,
    CONF_FILTER_LENGTH_FSK_MSK,
    CONF_FREEZE,
    CONF_FSK_DEVIATION,
    CONF_GDO0_PIN,
    CONF_HYST_LEVEL,
    CONF_IF_FREQUENCY,
    CONF_LNA_PRIORITY,
    CONF_MAGN_TARGET,
    CONF_MANCHESTER,
    CONF_MAX_DVGA_GAIN,
    CONF_MAX_LNA_GAIN,
    CONF_MODULATION_TYPE,
    CONF_MSK_DEVIATION,
    CONF_NUM_PREAMBLE,
    CONF_OUTPUT_POWER,
    CONF_PKTLEN,
    CONF_RX_ATTENUATION,
    CONF_SYMBOL_RATE,
    CONF_SYNC0,
    CONF_SYNC1,
    CONF_SYNC_MODE,
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
)

AUTO_LOAD = ["remote_base"]

ns = cg.esphome_ns.namespace("cc1101")

# Map of Config Key -> Validator
# The setter name is automatically inferred as "set_" + key
CONFIG_MAP = {
    # General
    CONF_OUTPUT_POWER: cv.float_range(min=-30.0, max=11.0),
    CONF_RX_ATTENUATION: cv.enum(RX_ATTENUATION, upper=False),
    CONF_DC_BLOCKING_FILTER: cv.boolean,
    # Tuner
    CONF_FREQUENCY: cv.float_range(min=300000.0, max=928000.0),
    CONF_IF_FREQUENCY: cv.float_range(min=25, max=788),
    CONF_FILTER_BANDWIDTH: cv.float_range(min=58.0, max=812.0),
    CONF_CHANNEL: cv.uint8_t,
    CONF_CHANNEL_SPACING: cv.float_range(min=25, max=405),
    CONF_FSK_DEVIATION: cv.float_range(min=1.5, max=381),
    CONF_MSK_DEVIATION: cv.int_range(min=1, max=8),
    CONF_SYMBOL_RATE: cv.float_range(min=600, max=500000),
    CONF_SYNC_MODE: cv.enum(SYNC_MODE, upper=False),
    CONF_CARRIER_SENSE_ABOVE_THRESHOLD: cv.boolean,
    CONF_MODULATION_TYPE: cv.enum(MODULATION, upper=False),
    CONF_MANCHESTER: cv.boolean,
    CONF_NUM_PREAMBLE: cv.int_range(min=0, max=7),
    CONF_SYNC1: cv.hex_uint8_t,
    CONF_SYNC0: cv.hex_uint8_t,
    CONF_PKTLEN: cv.uint8_t,
    # AGC
    CONF_MAGN_TARGET: cv.enum(MAGN_TARGET, upper=False),
    CONF_MAX_LNA_GAIN: cv.enum(MAX_LNA_GAIN, upper=False),
    CONF_MAX_DVGA_GAIN: cv.enum(MAX_DVGA_GAIN, upper=False),
    CONF_CARRIER_SENSE_ABS_THR: cv.int_range(min=-8, max=7),
    CONF_CARRIER_SENSE_REL_THR: cv.enum(CARRIER_SENSE_REL_THR, upper=False),
    CONF_LNA_PRIORITY: cv.boolean,
    CONF_FILTER_LENGTH_FSK_MSK: cv.enum(FILTER_LENGTH_FSK_MSK, upper=False),
    CONF_FILTER_LENGTH_ASK_OOK: cv.enum(FILTER_LENGTH_ASK_OOK, upper=False),
    CONF_FREEZE: cv.enum(FREEZE, upper=False),
    CONF_WAIT_TIME: cv.enum(WAIT_TIME, upper=False),
    CONF_HYST_LEVEL: cv.enum(HYST_LEVEL, upper=False),
}

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CC1101Component),
            cv.Optional(CONF_GDO0_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend({cv.Optional(key): validator for key, validator in CONFIG_MAP.items()})
    .extend(cv.COMPONENT_SCHEMA)
    .extend(spi.spi_device_schema(cs_pin_required=True))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)

    if CONF_GDO0_PIN in config:
        gdo0_pin = await cg.gpio_pin_expression(config[CONF_GDO0_PIN])
        cg.add(var.set_config_gdo0_pin(gdo0_pin))

    # Simplified loop: directly maps key -> set_key
    for key in CONFIG_MAP:
        if key in config:
            cg.add(getattr(var, f"set_{key}")(config[key]))


BeginTxAction = ns.class_("BeginTxAction", automation.Action)
EndTxAction = ns.class_("EndTxAction", automation.Action)
ResetAction = ns.class_("ResetAction", automation.Action)

CC1101_ACTION_SCHEMA = cv.Schema(
    maybe_simple_id(
        {
            cv.GenerateID(CONF_ID): cv.use_id(CC1101Component),
        }
    )
)


@automation.register_action("cc1101.begin_tx", BeginTxAction, CC1101_ACTION_SCHEMA)
@automation.register_action("cc1101.end_tx", EndTxAction, CC1101_ACTION_SCHEMA)
@automation.register_action("cc1101.reset", ResetAction, CC1101_ACTION_SCHEMA)
async def cc1101_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


CC1101_TRANSMIT_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID("cc1101_id"): cv.use_id(CC1101Component),
        }
    )
    .extend(remote_base.REMOTE_TRANSMITTABLE_SCHEMA)
    .extend(remote_base.RC_SWITCH_RAW_SCHEMA)
    .extend(remote_base.RC_SWITCH_TRANSMITTER)
)
