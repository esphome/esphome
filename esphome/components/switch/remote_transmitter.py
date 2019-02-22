import voluptuous as vol

from esphome.components import switch
from esphome.components.remote_transmitter import RC_SWITCH_RAW_SCHEMA, \
    RC_SWITCH_TYPE_A_SCHEMA, RC_SWITCH_TYPE_B_SCHEMA, RC_SWITCH_TYPE_C_SCHEMA, \
    RC_SWITCH_TYPE_D_SCHEMA, RemoteTransmitterComponent, binary_code, build_rc_switch_protocol, \
    remote_ns
import esphome.config_validation as cv
from esphome.const import CONF_ADDRESS, CONF_CARRIER_FREQUENCY, CONF_CHANNEL, CONF_CODE, \
    CONF_COMMAND, CONF_DATA, CONF_DEVICE, CONF_FAMILY, CONF_GROUP, CONF_ID, CONF_INVERTED, \
    CONF_JVC, \
    CONF_LG, CONF_NAME, CONF_NBITS, CONF_NEC, CONF_PANASONIC, CONF_PROTOCOL, CONF_RAW, \
    CONF_RC_SWITCH_RAW, CONF_RC_SWITCH_TYPE_A, CONF_RC_SWITCH_TYPE_B, CONF_RC_SWITCH_TYPE_C, \
    CONF_RC_SWITCH_TYPE_D, CONF_REPEAT, CONF_SAMSUNG, CONF_SONY, CONF_STATE, CONF_TIMES, \
    CONF_WAIT_TIME, CONF_RC5
from esphome.cpp_generator import Pvariable, add, get_variable, progmem_array
from esphome.cpp_types import int32

DEPENDENCIES = ['remote_transmitter']

REMOTE_KEYS = [CONF_JVC, CONF_NEC, CONF_LG, CONF_SAMSUNG, CONF_SONY, CONF_PANASONIC, CONF_RAW,
               CONF_RC_SWITCH_RAW, CONF_RC_SWITCH_TYPE_A, CONF_RC_SWITCH_TYPE_B,
               CONF_RC_SWITCH_TYPE_C, CONF_RC_SWITCH_TYPE_D, CONF_RC5]

CONF_REMOTE_TRANSMITTER_ID = 'remote_transmitter_id'
CONF_TRANSMITTER_ID = 'transmitter_id'

RemoteTransmitter = remote_ns.class_('RemoteTransmitter', switch.Switch)
JVCTransmitter = remote_ns.class_('JVCTransmitter', RemoteTransmitter)
LGTransmitter = remote_ns.class_('LGTransmitter', RemoteTransmitter)
NECTransmitter = remote_ns.class_('NECTransmitter', RemoteTransmitter)
PanasonicTransmitter = remote_ns.class_('PanasonicTransmitter', RemoteTransmitter)
RawTransmitter = remote_ns.class_('RawTransmitter', RemoteTransmitter)
RC5Transmitter = remote_ns.class_('RC5Transmitter', RemoteTransmitter)
SamsungTransmitter = remote_ns.class_('SamsungTransmitter', RemoteTransmitter)
SonyTransmitter = remote_ns.class_('SonyTransmitter', RemoteTransmitter)
RCSwitchRawTransmitter = remote_ns.class_('RCSwitchRawTransmitter', RemoteTransmitter)
RCSwitchTypeATransmitter = remote_ns.class_('RCSwitchTypeATransmitter', RCSwitchRawTransmitter)
RCSwitchTypeBTransmitter = remote_ns.class_('RCSwitchTypeBTransmitter', RCSwitchRawTransmitter)
RCSwitchTypeCTransmitter = remote_ns.class_('RCSwitchTypeCTransmitter', RCSwitchRawTransmitter)
RCSwitchTypeDTransmitter = remote_ns.class_('RCSwitchTypeDTransmitter', RCSwitchRawTransmitter)


def validate_raw(value):
    if isinstance(value, dict):
        return vol.Schema({
            cv.GenerateID(): cv.declare_variable_id(int32),
            vol.Required(CONF_DATA): [vol.Any(vol.Coerce(int), cv.time_period_microseconds)],
            vol.Optional(CONF_CARRIER_FREQUENCY): vol.All(cv.frequency, vol.Coerce(int)),
        })(value)
    return validate_raw({
        CONF_DATA: value
    })


PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(RemoteTransmitter),
    vol.Optional(CONF_JVC): vol.Schema({
        vol.Required(CONF_DATA): cv.hex_uint32_t,
    }),
    vol.Optional(CONF_LG): vol.Schema({
        vol.Required(CONF_DATA): cv.hex_uint32_t,
        vol.Optional(CONF_NBITS, default=28): cv.one_of(28, 32, int=True),
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
        vol.Optional(CONF_NBITS, default=12): cv.one_of(12, 15, 20, int=True),
    }),
    vol.Optional(CONF_PANASONIC): vol.Schema({
        vol.Required(CONF_ADDRESS): cv.hex_uint16_t,
        vol.Required(CONF_COMMAND): cv.hex_uint32_t,
    }),
    vol.Optional(CONF_RC5): vol.Schema({
        vol.Required(CONF_ADDRESS): vol.All(cv.hex_int, vol.Range(min=0, max=0x1F)),
        vol.Required(CONF_COMMAND): vol.All(cv.hex_int, vol.Range(min=0, max=0x3F)),
    }),
    vol.Optional(CONF_RAW): validate_raw,
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

    if key == CONF_JVC:
        return JVCTransmitter.new(name, config[CONF_DATA])
    if key == CONF_LG:
        return LGTransmitter.new(name, config[CONF_DATA], config[CONF_NBITS])
    if key == CONF_NEC:
        return NECTransmitter.new(name, config[CONF_ADDRESS], config[CONF_COMMAND])
    if key == CONF_PANASONIC:
        return PanasonicTransmitter.new(name, config[CONF_ADDRESS], config[CONF_COMMAND])
    if key == CONF_SAMSUNG:
        return SamsungTransmitter.new(name, config[CONF_DATA])
    if key == CONF_SONY:
        return SonyTransmitter.new(name, config[CONF_DATA], config[CONF_NBITS])
    if key == CONF_RC5:
        return RC5Transmitter.new(name, config[CONF_ADDRESS], config[CONF_COMMAND])
    if key == CONF_RAW:
        arr = progmem_array(config[CONF_ID], config[CONF_DATA])
        return RawTransmitter.new(name, arr, len(config[CONF_DATA]),
                                  config.get(CONF_CARRIER_FREQUENCY))
    if key == CONF_RC_SWITCH_RAW:
        return RCSwitchRawTransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                          binary_code(config[CONF_CODE]), len(config[CONF_CODE]))
    if key == CONF_RC_SWITCH_TYPE_A:
        return RCSwitchTypeATransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                            binary_code(config[CONF_GROUP]),
                                            binary_code(config[CONF_DEVICE]),
                                            config[CONF_STATE])
    if key == CONF_RC_SWITCH_TYPE_B:
        return RCSwitchTypeBTransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                            config[CONF_ADDRESS], config[CONF_CHANNEL],
                                            config[CONF_STATE])
    if key == CONF_RC_SWITCH_TYPE_C:
        return RCSwitchTypeCTransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                            ord(config[CONF_FAMILY][0]) - ord('a'),
                                            config[CONF_GROUP], config[CONF_DEVICE],
                                            config[CONF_STATE])
    if key == CONF_RC_SWITCH_TYPE_D:
        return RCSwitchTypeDTransmitter.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                            ord(config[CONF_GROUP][0]) - ord('a'),
                                            config[CONF_DEVICE], config[CONF_STATE])

    raise NotImplementedError("Unknown transmitter type {}".format(config))


def to_code(config):
    for remote in get_variable(config[CONF_REMOTE_TRANSMITTER_ID]):
        yield
    rhs = transmitter_base(config)
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


def to_hass_config(data, config):
    return switch.core_to_hass_config(data, config)
