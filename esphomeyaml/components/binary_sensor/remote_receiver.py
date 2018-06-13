import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.components.remote_receiver import RemoteReceiverComponent, remote_ns
from esphomeyaml.const import CONF_ADDRESS, CONF_COMMAND, CONF_DATA, \
    CONF_LG, CONF_NAME, CONF_NBITS, CONF_NEC, CONF_PANASONIC, CONF_RAW, CONF_SONY
from esphomeyaml.helpers import ArrayInitializer, Pvariable, get_variable

DEPENDENCIES = ['remote_receiver']

IR_KEYS = [CONF_NEC, CONF_LG, CONF_SONY, CONF_PANASONIC, CONF_RAW]

CONF_REMOTE_RECEIVER_ID = 'remote_receiver_id'
CONF_RECEIVER_ID = 'receiver_id'

RemoteReceiver = remote_ns.RemoteReceiver
LGReceiver = remote_ns.LGReceiver
NECReceiver = remote_ns.NECReceiver
PanasonicReceiver = remote_ns.PanasonicReceiver
RawReceiver = remote_ns.RawReceiver
SonyReceiver = remote_ns.SonyReceiver

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
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
    vol.Optional(CONF_RAW): [vol.Any(vol.Coerce(int), cv.time_period_microseconds)],
    cv.GenerateID(CONF_REMOTE_RECEIVER_ID): cv.use_variable_id(RemoteReceiverComponent),
    cv.GenerateID(CONF_RECEIVER_ID): cv.declare_variable_id(RemoteReceiver),
}), cv.has_exactly_one_key(*IR_KEYS))


def receiver_base(config):
    if CONF_LG in config:
        conf = config[CONF_LG]
        return LGReceiver.new(config[CONF_NAME], conf[CONF_DATA], conf[CONF_NBITS])
    elif CONF_NEC in config:
        conf = config[CONF_NEC]
        return NECReceiver.new(config[CONF_NAME], conf[CONF_ADDRESS], conf[CONF_COMMAND])
    elif CONF_PANASONIC in config:
        conf = config[CONF_PANASONIC]
        return PanasonicReceiver.new(config[CONF_NAME], conf[CONF_ADDRESS], conf[CONF_COMMAND])
    elif CONF_SONY in config:
        conf = config[CONF_SONY]
        return SonyReceiver.new(config[CONF_NAME], conf[CONF_DATA], conf[CONF_NBITS])
    elif CONF_RAW in config:
        data = ArrayInitializer(*config[CONF_RAW], multiline=False)
        return RawReceiver.new(config[CONF_NAME], data)
    else:
        raise ValueError("Unknown receiver type {}".format(config))


def to_code(config):
    remote = None
    for remote in get_variable(config[CONF_REMOTE_RECEIVER_ID]):
        yield
    rhs = receiver_base(config)
    receiver = Pvariable(config[CONF_RECEIVER_ID], rhs)

    binary_sensor.register_binary_sensor(remote.add_decoder(receiver), config)


BUILD_FLAGS = '-DUSE_REMOTE_RECEIVER'
