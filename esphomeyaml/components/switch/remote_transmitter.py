import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.components.remote_transmitter import RemoteTransmitterComponent, remote_ns
from esphomeyaml.const import CONF_ADDRESS, CONF_CARRIER_FREQUENCY, CONF_COMMAND, CONF_DATA, \
    CONF_INVERTED, CONF_LG, CONF_NAME, CONF_NBITS, CONF_NEC, \
    CONF_PANASONIC, CONF_RAW, CONF_REPEAT, CONF_SONY, CONF_TIMES, CONF_WAIT_TIME
from esphomeyaml.helpers import App, ArrayInitializer, Pvariable, add, get_variable

DEPENDENCIES = ['remote_transmitter']

IR_KEYS = [CONF_NEC, CONF_LG, CONF_SONY, CONF_PANASONIC, CONF_RAW]

CONF_REMOTE_TRANSMITTER_ID = 'remote_transmitter_id'
CONF_TRANSMITTER_ID = 'transmitter_id'

RemoteTransmitter = remote_ns.RemoteTransmitter
LGTransmitter = remote_ns.LGTransmitter
NECTransmitter = remote_ns.NECTransmitter
PanasonicTransmitter = remote_ns.PanasonicTransmitter
RawTransmitter = remote_ns.RawTransmitter
SonyTransmitter = remote_ns.SonyTransmitter

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_LG): vol.Schema({
        vol.Required(CONF_DATA): cv.hex_uint32_t,
        vol.Optional(CONF_NBITS, default=28): vol.All(vol.Coerce(int), cv.one_of(28, 32)),
    }),
    vol.Optional(CONF_NEC): vol.Schema({
        vol.Required(CONF_ADDRESS): cv.hex_uint16_t,
        vol.Required(CONF_COMMAND): cv.hex_uint16_t,
    }),
    vol.Optional(CONF_SONY): vol.Schema({
        vol.Required(CONF_DATA): cv.hex_uint32_t,
        vol.Optional(CONF_NBITS, default=12): vol.All(vol.Coerce(int), cv.one_of(12, 15, 20)),
    }),
    vol.Optional(CONF_PANASONIC): vol.Schema({
        vol.Required(CONF_ADDRESS): cv.hex_uint16_t,
        vol.Required(CONF_COMMAND): cv.hex_uint32_t,
    }),
    vol.Optional(CONF_RAW): vol.Schema({
        vol.Required(CONF_DATA): [vol.Any(vol.Coerce(int), cv.time_period_microseconds)],
        vol.Optional(CONF_CARRIER_FREQUENCY): vol.All(cv.frequency, vol.Coerce(int)),
    }),
    vol.Optional(CONF_REPEAT): vol.Any(cv.positive_not_null_int, vol.Schema({
        vol.Required(CONF_TIMES): cv.positive_not_null_int,
        vol.Required(CONF_WAIT_TIME): cv.positive_time_period_microseconds,
    })),
    cv.GenerateID(CONF_REMOTE_TRANSMITTER_ID): cv.use_variable_id(RemoteTransmitterComponent),
    cv.GenerateID(CONF_TRANSMITTER_ID): cv.declare_variable_id(RemoteTransmitter),

    vol.Optional(CONF_INVERTED): cv.invalid("Remote Transmitters do not support inverted mode!"),
}), cv.has_exactly_one_key(*IR_KEYS))


def transmitter_base(config):
    if CONF_LG in config:
        conf = config[CONF_LG]
        return LGTransmitter.new(config[CONF_NAME], conf[CONF_DATA], conf[CONF_NBITS])
    elif CONF_NEC in config:
        conf = config[CONF_NEC]
        return NECTransmitter.new(config[CONF_NAME], conf[CONF_ADDRESS], conf[CONF_COMMAND])
    elif CONF_PANASONIC in config:
        conf = config[CONF_PANASONIC]
        return PanasonicTransmitter.new(config[CONF_NAME], conf[CONF_ADDRESS], conf[CONF_COMMAND])
    elif CONF_SONY in config:
        conf = config[CONF_SONY]
        return SonyTransmitter.new(config[CONF_NAME], conf[CONF_DATA], conf[CONF_NBITS])
    elif CONF_RAW in config:
        conf = config[CONF_RAW]
        data = ArrayInitializer(*conf[CONF_DATA])
        return RawTransmitter.new(data, conf[CONF_CARRIER_FREQUENCY])
    else:
        raise ValueError("Unknown transmitter type {}".format(config))


def to_code(config):
    remote = None
    for remote in get_variable(config[CONF_REMOTE_TRANSMITTER_ID]):
        yield
    rhs = App.register_component(transmitter_base(config))
    transmitter = Pvariable(config[CONF_TRANSMITTER_ID], rhs)

    if CONF_REPEAT in config:
        if isinstance(config[CONF_REPEAT], int):
            times = config[CONF_REPEAT]
            wait_us = 1000
        else:
            times = config[CONF_REPEAT][CONF_TIMES]
            wait_us = config[CONF_REPEAT][CONF_WAIT_TIME]
        add(transmitter.set_repeat(times, wait_us))

    switch.register_switch(remote.add_transmitter(transmitter), config)


BUILD_FLAGS = '-DUSE_REMOTE_TRANSMITTER'
