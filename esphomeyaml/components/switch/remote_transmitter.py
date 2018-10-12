import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch
from esphomeyaml.components.remote_transmitter import RC_SWITCH_RAW_SCHEMA, \
    RC_SWITCH_TYPE_A_SCHEMA, RC_SWITCH_TYPE_B_SCHEMA, RC_SWITCH_TYPE_C_SCHEMA, \
    RC_SWITCH_TYPE_D_SCHEMA, RemoteTransmitterComponent, binary_code, build_rc_switch_protocol, \
    remote_ns
from esphomeyaml.const import CONF_ADDRESS, CONF_CARRIER_FREQUENCY, CONF_CHANNEL, CONF_CODE, \
    CONF_COMMAND, CONF_DATA, CONF_DEVICE, CONF_FAMILY, CONF_GROUP, CONF_INVERTED, CONF_LG, \
    CONF_NAME, CONF_NBITS, CONF_NEC, CONF_PANASONIC, CONF_PROTOCOL, CONF_RAW, CONF_RC_SWITCH_RAW, \
    CONF_RC_SWITCH_TYPE_A, CONF_RC_SWITCH_TYPE_B, CONF_RC_SWITCH_TYPE_C, CONF_RC_SWITCH_TYPE_D, \
    CONF_REPEAT, CONF_SAMSUNG, CONF_SONY, CONF_STATE, CONF_TIMES, \
    CONF_WAIT_TIME
from esphomeyaml.helpers import App, ArrayInitializer, Pvariable, add, get_variable

DEPENDENCIES = ['remote_transmitter']

REMOTE_KEYS = [CONF_NEC, CONF_LG, CONF_SAMSUNG, CONF_SONY, CONF_PANASONIC, CONF_RAW,
               CONF_RC_SWITCH_RAW, CONF_RC_SWITCH_TYPE_A, CONF_RC_SWITCH_TYPE_B,
               CONF_RC_SWITCH_TYPE_C, CONF_RC_SWITCH_TYPE_D]

CONF_REMOTE_TRANSMITTER_ID = 'remote_transmitter_id'
CONF_TRANSMITTER_ID = 'transmitter_id'

RemoteTransmitter = remote_ns.RemoteTransmitter
LGTransmitter = remote_ns.LGTransmitter
NECTransmitter = remote_ns.NECTransmitter
PanasonicTransmitter = remote_ns.PanasonicTransmitter
RawTransmitter = remote_ns.RawTransmitter
SamsungTransmitter = remote_ns.SamsungTransmitter
SonyTransmitter = remote_ns.SonyTransmitter
RCSwitchRawTransmitter = remote_ns.RCSwitchRawTransmitter
RCSwitchTypeATransmitter = remote_ns.RCSwitchTypeATransmitter
RCSwitchTypeBTransmitter = remote_ns.RCSwitchTypeBTransmitter
RCSwitchTypeCTransmitter = remote_ns.RCSwitchTypeCTransmitter
RCSwitchTypeDTransmitter = remote_ns.RCSwitchTypeDTransmitter

validate_raw_data = [vol.Any(vol.Coerce(int), cv.time_period_microseconds)]

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_LG): vol.Schema({
        vol.Required(CONF_DATA): cv.hex_uint32_t,
        vol.Optional(CONF_NBITS, default=28): vol.All(vol.Coerce(int), cv.one_of(28, 32)),
    }),
    vol.Optional(CONF_NEC): vol.Schema({
        vol.Required(CONF_ADDRESS): cv.hex_uint16_t,
        vol.Required(CONF_COMMAND): cv.hex_uint16_t,
    }),
    vol.Optional(CONF_SAMSUNG): vol.Schema({
        vol.Required(CONF_DATA): cv.hex_uint32_t,
    }),
    vol.Optional(CONF_SONY): vol.Schema({
        vol.Required(CONF_DATA): cv.hex_uint32_t,
        vol.Optional(CONF_NBITS, default=12): vol.All(vol.Coerce(int), cv.one_of(12, 15, 20)),
    }),
    vol.Optional(CONF_PANASONIC): vol.Schema({
        vol.Required(CONF_ADDRESS): cv.hex_uint16_t,
        vol.Required(CONF_COMMAND): cv.hex_uint32_t,
    }),
    vol.Optional(CONF_RAW): vol.Any(validate_raw_data, vol.Schema({
        vol.Required(CONF_DATA): validate_raw_data,
        vol.Optional(CONF_CARRIER_FREQUENCY): vol.All(cv.frequency, vol.Coerce(int)),
    })),
    vol.Optional(CONF_RC_SWITCH_RAW): RC_SWITCH_RAW_SCHEMA,
    vol.Optional(CONF_RC_SWITCH_TYPE_A): RC_SWITCH_TYPE_A_SCHEMA,
    vol.Optional(CONF_RC_SWITCH_TYPE_B): RC_SWITCH_TYPE_B_SCHEMA,
    vol.Optional(CONF_RC_SWITCH_TYPE_C): RC_SWITCH_TYPE_C_SCHEMA,
    vol.Optional(CONF_RC_SWITCH_TYPE_D): RC_SWITCH_TYPE_D_SCHEMA,

    vol.Optional(CONF_REPEAT): vol.Any(cv.positive_not_null_int, vol.Schema({
        vol.Required(CONF_TIMES): cv.positive_not_null_int,
        vol.Required(CONF_WAIT_TIME): cv.positive_time_period_microseconds,
    })),
    cv.GenerateID(CONF_REMOTE_TRANSMITTER_ID): cv.use_variable_id(RemoteTransmitterComponent),
    cv.GenerateID(CONF_TRANSMITTER_ID): cv.declare_variable_id(RemoteTransmitter),
    vol.Optional(CONF_INVERTED): cv.invalid("Remote Transmitters do not support inverted mode!"),
}), cv.has_exactly_one_key(*REMOTE_KEYS))


def transmitter_base(full_config):
    name = full_config[CONF_NAME]
    key, config = next((k, v) for k, v in full_config.items() if k in REMOTE_KEYS)

    if key == CONF_LG:
        return LGTransmitter.new(name, config[CONF_DATA], config[CONF_NBITS])
    elif key == CONF_NEC:
        return NECTransmitter.new(name, config[CONF_ADDRESS], config[CONF_COMMAND])
    elif key == CONF_PANASONIC:
        return PanasonicTransmitter.new(name, config[CONF_ADDRESS], config[CONF_COMMAND])
    elif key == CONF_SAMSUNG:
        return SamsungTransmitter.new(name, config[CONF_DATA])
    elif key == CONF_SONY:
        return SonyTransmitter.new(name, config[CONF_DATA], config[CONF_NBITS])
    elif key == CONF_RAW:
        if isinstance(config, dict):
            data = config[CONF_DATA]
            carrier_frequency = config.get(CONF_CARRIER_FREQUENCY)
        else:
            data = config
            carrier_frequency = None
        return RawTransmitter.new(name, ArrayInitializer(*data, multiline=False),
                                  carrier_frequency)
    elif key == CONF_RC_SWITCH_RAW:
        return RCSwitchRawTransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                          binary_code(config[CONF_CODE]), len(config[CONF_CODE]))
    elif key == CONF_RC_SWITCH_TYPE_A:
        return RCSwitchTypeATransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                            binary_code(config[CONF_GROUP]),
                                            binary_code(config[CONF_DEVICE]),
                                            config[CONF_STATE])
    elif key == CONF_RC_SWITCH_TYPE_B:
        return RCSwitchTypeBTransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                            config[CONF_ADDRESS], config[CONF_CHANNEL],
                                            config[CONF_STATE])
    elif key == CONF_RC_SWITCH_TYPE_C:
        return RCSwitchTypeCTransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                            ord(config[CONF_FAMILY][0]) - ord('a'),
                                            config[CONF_GROUP], config[CONF_DEVICE],
                                            config[CONF_STATE])
    elif key == CONF_RC_SWITCH_TYPE_D:
        return RCSwitchTypeDTransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                            ord(config[CONF_GROUP][0]) - ord('a'),
                                            config[CONF_DEVICE], config[CONF_STATE])
    else:
        raise NotImplementedError("Unknown transmitter type {}".format(config))


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
