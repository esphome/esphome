import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.components.ir_transmitter import IRTransmitterComponent
from esphomeyaml.const import CONF_ADDRESS, CONF_CARRIER_FREQUENCY, CONF_COMMAND, CONF_DATA, \
    CONF_INVERTED, CONF_IR_TRANSMITTER_ID, CONF_LG, CONF_NAME, CONF_NBITS, CONF_NEC, \
    CONF_PANASONIC, CONF_RAW, CONF_REPEAT, CONF_SONY, CONF_TIMES, CONF_WAIT_TIME
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import App, ArrayInitializer, HexIntLiteral, get_variable

DEPENDENCIES = ['ir_transmitter']

IR_KEYS = [CONF_NEC, CONF_LG, CONF_SONY, CONF_PANASONIC, CONF_RAW]

WAIT_TIME_MESSAGE = "The wait_time_us option has been renamed to wait_time in order to decrease " \
                    "ambiguity. "

PLATFORM_SCHEMA = vol.All(switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID('ir_transmitter_switch'): cv.register_variable_id,
    vol.Exclusive(CONF_NEC, 'code'): vol.Schema({
        vol.Required(CONF_ADDRESS): cv.hex_uint16_t,
        vol.Required(CONF_COMMAND): cv.hex_uint16_t,
    }),
    vol.Exclusive(CONF_LG, 'code'): vol.Schema({
        vol.Required(CONF_DATA): cv.hex_uint32_t,
        vol.Optional(CONF_NBITS, default=28): vol.All(vol.Coerce(int), vol.Range(min=0, max=32)),
    }),
    vol.Exclusive(CONF_SONY, 'code'): vol.Schema({
        vol.Required(CONF_DATA): cv.hex_uint32_t,
        vol.Optional(CONF_NBITS, default=12): vol.All(vol.Coerce(int), vol.Range(min=0, max=32)),
    }),
    vol.Exclusive(CONF_PANASONIC, 'code'): vol.Schema({
        vol.Required(CONF_ADDRESS): cv.hex_uint16_t,
        vol.Required(CONF_COMMAND): cv.hex_uint32_t,
    }),
    vol.Exclusive(CONF_RAW, 'code'): vol.Schema({
        vol.Required(CONF_CARRIER_FREQUENCY): vol.All(cv.frequency, vol.Coerce(int)),
        vol.Required(CONF_DATA): [vol.Coerce(int)],
    }),
    vol.Optional(CONF_REPEAT): vol.Any(cv.positive_not_null_int, vol.Schema({
        vol.Required(CONF_TIMES): cv.positive_not_null_int,
        vol.Required(CONF_WAIT_TIME): cv.positive_time_period_microseconds,

        vol.Optional('wait_time_us'): cv.invalid(WAIT_TIME_MESSAGE),
    })),
    vol.Optional(CONF_IR_TRANSMITTER_ID): cv.variable_id,
    vol.Optional(CONF_INVERTED): cv.invalid("IR Transmitters do not support inverted mode!"),
}).extend(switch.SWITCH_SCHEMA.schema), cv.has_at_least_one_key(*IR_KEYS))

# pylint: disable=invalid-name
ir_ns = switch.switch_ns.namespace('ir')
SendData = ir_ns.namespace('SendData')
DataTransmitter = IRTransmitterComponent.DataTransmitter


def safe_hex(value):
    if value is None:
        return None
    return HexIntLiteral(value)


def exp_send_data(config):
    if CONF_NEC in config:
        conf = config[CONF_NEC]
        base = SendData.from_nec(safe_hex(conf[CONF_ADDRESS]),
                                 safe_hex(conf[CONF_COMMAND]))
    elif CONF_LG in config:
        conf = config[CONF_LG]
        base = SendData.from_lg(safe_hex(conf[CONF_DATA]), conf.get(CONF_NBITS))
    elif CONF_SONY in config:
        conf = config[CONF_SONY]
        base = SendData.from_sony(safe_hex(conf[CONF_DATA]), conf.get(CONF_NBITS))
    elif CONF_PANASONIC in config:
        conf = config[CONF_PANASONIC]
        base = SendData.from_panasonic(safe_hex(conf[CONF_ADDRESS]),
                                       safe_hex(conf[CONF_COMMAND]))
    elif CONF_RAW in config:
        conf = config[CONF_RAW]
        data = ArrayInitializer(*conf[CONF_DATA])
        base = SendData.from_raw(data, conf[CONF_CARRIER_FREQUENCY])
    else:
        raise ESPHomeYAMLError(u"Unsupported IR mode {}".format(config))

    if CONF_REPEAT in config:
        if isinstance(config[CONF_REPEAT], int):
            times = config[CONF_REPEAT]
            wait_us = None
        else:
            times = config[CONF_REPEAT][CONF_TIMES]
            wait_us = config[CONF_REPEAT][CONF_WAIT_TIME]
        base = base.repeat(times, wait_us)
    return base


def to_code(config):
    ir = get_variable(config.get(CONF_IR_TRANSMITTER_ID), IRTransmitterComponent)
    send_data = exp_send_data(config)
    rhs = App.register_component(ir.create_transmitter(config[CONF_NAME], send_data))
    switch.register_switch(rhs, config)


BUILD_FLAGS = '-DUSE_IR_TRANSMITTER'
