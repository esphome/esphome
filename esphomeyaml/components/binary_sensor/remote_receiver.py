import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.components.remote_receiver import RemoteReceiverComponent, remote_ns
from esphomeyaml.components.remote_transmitter import RC_SWITCH_RAW_SCHEMA, \
    RC_SWITCH_TYPE_A_SCHEMA, RC_SWITCH_TYPE_B_SCHEMA, RC_SWITCH_TYPE_C_SCHEMA, \
    RC_SWITCH_TYPE_D_SCHEMA, binary_code, build_rc_switch_protocol
from esphomeyaml.const import CONF_ADDRESS, CONF_CHANNEL, CONF_CODE, CONF_COMMAND, CONF_DATA, \
    CONF_DEVICE, CONF_FAMILY, CONF_GROUP, CONF_LG, CONF_NAME, CONF_NBITS, CONF_NEC, \
    CONF_PANASONIC, CONF_PROTOCOL, CONF_RAW, CONF_RC_SWITCH_RAW, CONF_RC_SWITCH_TYPE_A, \
    CONF_RC_SWITCH_TYPE_B, CONF_RC_SWITCH_TYPE_C, CONF_RC_SWITCH_TYPE_D, CONF_SAMSUNG, CONF_SONY, \
    CONF_STATE
from esphomeyaml.helpers import ArrayInitializer, Pvariable, get_variable

DEPENDENCIES = ['remote_receiver']

REMOTE_KEYS = [CONF_NEC, CONF_LG, CONF_SONY, CONF_PANASONIC, CONF_SAMSUNG, CONF_RAW,
               CONF_RC_SWITCH_RAW, CONF_RC_SWITCH_TYPE_A, CONF_RC_SWITCH_TYPE_B,
               CONF_RC_SWITCH_TYPE_C, CONF_RC_SWITCH_TYPE_D]

CONF_REMOTE_RECEIVER_ID = 'remote_receiver_id'
CONF_RECEIVER_ID = 'receiver_id'

RemoteReceiver = remote_ns.class_('RemoteReceiver', binary_sensor.BinarySensor)
LGReceiver = remote_ns.class_('LGReceiver', RemoteReceiver)
NECReceiver = remote_ns.class_('NECReceiver', RemoteReceiver)
PanasonicReceiver = remote_ns.class_('PanasonicReceiver', RemoteReceiver)
RawReceiver = remote_ns.class_('RawReceiver', RemoteReceiver)
SamsungReceiver = remote_ns.class_('SamsungReceiver', RemoteReceiver)
SonyReceiver = remote_ns.class_('SonyReceiver', RemoteReceiver)
RCSwitchRawReceiver = remote_ns.class_('RCSwitchRawReceiver', RemoteReceiver)
RCSwitchTypeAReceiver = remote_ns.class_('RCSwitchTypeAReceiver', RCSwitchRawReceiver)
RCSwitchTypeBReceiver = remote_ns.class_('RCSwitchTypeBReceiver', RCSwitchRawReceiver)
RCSwitchTypeCReceiver = remote_ns.class_('RCSwitchTypeCReceiver', RCSwitchRawReceiver)
RCSwitchTypeDReceiver = remote_ns.class_('RCSwitchTypeDReceiver', RCSwitchRawReceiver)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(RemoteReceiver),
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
    vol.Optional(CONF_RAW): [vol.Any(vol.Coerce(int), cv.time_period_microseconds)],
    vol.Optional(CONF_RC_SWITCH_RAW): RC_SWITCH_RAW_SCHEMA,
    vol.Optional(CONF_RC_SWITCH_TYPE_A): RC_SWITCH_TYPE_A_SCHEMA,
    vol.Optional(CONF_RC_SWITCH_TYPE_B): RC_SWITCH_TYPE_B_SCHEMA,
    vol.Optional(CONF_RC_SWITCH_TYPE_C): RC_SWITCH_TYPE_C_SCHEMA,
    vol.Optional(CONF_RC_SWITCH_TYPE_D): RC_SWITCH_TYPE_D_SCHEMA,

    cv.GenerateID(CONF_REMOTE_RECEIVER_ID): cv.use_variable_id(RemoteReceiverComponent),
    cv.GenerateID(CONF_RECEIVER_ID): cv.declare_variable_id(RemoteReceiver),
}), cv.has_exactly_one_key(*REMOTE_KEYS))


def receiver_base(full_config):
    name = full_config[CONF_NAME]
    key, config = next((k, v) for k, v in full_config.items() if k in REMOTE_KEYS)
    if key == CONF_LG:
        return LGReceiver.new(name, config[CONF_DATA], config[CONF_NBITS])
    elif key == CONF_NEC:
        return NECReceiver.new(name, config[CONF_ADDRESS], config[CONF_COMMAND])
    elif key == CONF_PANASONIC:
        return PanasonicReceiver.new(name, config[CONF_ADDRESS], config[CONF_COMMAND])
    elif key == CONF_SAMSUNG:
        return SamsungReceiver.new(name, config[CONF_DATA])
    elif key == CONF_SONY:
        return SonyReceiver.new(name, config[CONF_DATA], config[CONF_NBITS])
    elif key == CONF_RAW:
        data = ArrayInitializer(*config, multiline=False)
        return RawReceiver.new(name, data)
    elif key == CONF_RC_SWITCH_RAW:
        return RCSwitchRawReceiver.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                       binary_code(config[CONF_CODE]), len(config[CONF_CODE]))
    elif key == CONF_RC_SWITCH_TYPE_A:
        return RCSwitchTypeAReceiver.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                         binary_code(config[CONF_GROUP]),
                                         binary_code(config[CONF_DEVICE]),
                                         config[CONF_STATE])
    elif key == CONF_RC_SWITCH_TYPE_B:
        return RCSwitchTypeBReceiver.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                         config[CONF_ADDRESS], config[CONF_CHANNEL],
                                         config[CONF_STATE])
    elif key == CONF_RC_SWITCH_TYPE_C:
        return RCSwitchTypeCReceiver.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                         ord(config[CONF_FAMILY][0]) - ord('a'),
                                         config[CONF_GROUP], config[CONF_DEVICE],
                                         config[CONF_STATE])
    elif key == CONF_RC_SWITCH_TYPE_D:
        return RCSwitchTypeDReceiver.new(name, build_rc_switch_protocol(config[CONF_PROTOCOL]),
                                         ord(config[CONF_GROUP][0]) - ord('a'),
                                         config[CONF_DEVICE], config[CONF_STATE])
    else:
        raise NotImplementedError("Unknown receiver type {}".format(config))


def to_code(config):
    for remote in get_variable(config[CONF_REMOTE_RECEIVER_ID]):
        yield
    rhs = receiver_base(config)
    receiver = Pvariable(config[CONF_RECEIVER_ID], rhs)

    binary_sensor.register_binary_sensor(remote.add_decoder(receiver), config)


BUILD_FLAGS = '-DUSE_REMOTE_RECEIVER'


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
