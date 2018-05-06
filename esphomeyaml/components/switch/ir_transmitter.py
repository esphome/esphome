import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.components.ir_transmitter import IR_TRANSMITTER_COMPONENT_CLASS
from esphomeyaml.const import CONF_ADDRESS, CONF_COMMAND, CONF_DATA, CONF_IR_TRANSMITTER_ID, \
    CONF_LG, CONF_NBITS, CONF_NEC, CONF_PANASONIC, CONF_REPEAT, CONF_SONY, CONF_TIMES, \
    CONF_WAIT_TIME_US, CONF_RAW, CONF_CARRIER_FREQUENCY, CONF_NAME, CONF_ID
from esphomeyaml.core import ESPHomeYAMLError
from esphomeyaml.helpers import HexIntLiteral, MockObj, get_variable, ArrayInitializer, Pvariable

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
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
        vol.Required(CONF_WAIT_TIME_US): cv.uint32_t,
    })),
    vol.Optional(CONF_IR_TRANSMITTER_ID): cv.variable_id,
}).extend(switch.MQTT_SWITCH_ID_SCHEMA.schema)


# pylint: disable=invalid-name
SendData = MockObj('switch_::ir::SendData', '::')


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
            wait_us = config[CONF_REPEAT][CONF_WAIT_TIME_US]
        base = MockObj(unicode(base), u'.')
        base = base.repeat(times, wait_us)
    return base


def to_code(config):
    ir = get_variable(config.get(CONF_IR_TRANSMITTER_ID), IR_TRANSMITTER_COMPONENT_CLASS)
    send_data = exp_send_data(config)
    rhs = ir.create_transmitter(config[CONF_NAME], send_data)
    switch_ = Pvariable(IR_TRANSMITTER_COMPONENT_CLASS + '::DataTransmitter', config[CONF_ID],
                        rhs)
    switch.register_switch(switch_, config)


BUILD_FLAGS = '-DUSE_IR_TRANSMITTER'
