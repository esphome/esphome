import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_ADDRESS, CONF_CARRIER_DUTY_PERCENT, CONF_CHANNEL, CONF_CODE, \
    CONF_DEVICE, CONF_FAMILY, CONF_GROUP, CONF_ID, CONF_INVERTED, CONF_ONE, CONF_PIN, \
    CONF_PROTOCOL, CONF_PULSE_LENGTH, CONF_STATE, CONF_SYNC, CONF_ZERO
from esphomeyaml.core import HexInt
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns, gpio_output_pin_expression

remote_ns = esphomelib_ns.namespace('remote')

RemoteTransmitterComponent = remote_ns.RemoteTransmitterComponent
RCSwitchProtocol = remote_ns.RCSwitchProtocol
rc_switch_protocols = remote_ns.rc_switch_protocols


def validate_rc_switch_code(value):
    if not isinstance(value, (str, unicode)):
        raise vol.Invalid("All RCSwitch codes must be in quotes ('')")
    for c in value:
        if c not in ('0', '1'):
            raise vol.Invalid(u"Invalid RCSwitch code character '{}'. Only '0' and '1' are allowed"
                              u"".format(c))
    if len(value) > 32:
        raise vol.Invalid("Maximum length for RCSwitch codes is 32, code '{}' has length {}"
                          "".format(value, len(value)))
    if not value:
        raise vol.Invalid("RCSwitch code must not be empty")
    return value


RC_SWITCH_TIMING_SCHEMA = vol.All([cv.uint8_t], vol.Length(min=2, max=2))

RC_SWITCH_PROTOCOL_SCHEMA = vol.Any(
    vol.All(vol.Coerce(int), vol.Range(min=1, max=7)),
    vol.Schema({
        vol.Required(CONF_PULSE_LENGTH): cv.uint32_t,
        vol.Optional(CONF_SYNC, default=[1, 31]): RC_SWITCH_TIMING_SCHEMA,
        vol.Optional(CONF_ZERO, default=[1, 3]): RC_SWITCH_TIMING_SCHEMA,
        vol.Optional(CONF_ONE, default=[3, 1]): RC_SWITCH_TIMING_SCHEMA,
        vol.Optional(CONF_INVERTED, default=False): cv.boolean,
    })
)

RC_SWITCH_RAW_SCHEMA = vol.Schema({
    vol.Required(CONF_CODE): validate_rc_switch_code,
    vol.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})
RC_SWITCH_TYPE_A_SCHEMA = vol.Schema({
    vol.Required(CONF_GROUP): vol.All(validate_rc_switch_code, vol.Length(min=5, max=5)),
    vol.Required(CONF_DEVICE): vol.All(validate_rc_switch_code, vol.Length(min=5, max=5)),
    vol.Required(CONF_STATE): cv.boolean,
    vol.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})
RC_SWITCH_TYPE_B_SCHEMA = vol.Schema({
    vol.Required(CONF_ADDRESS): vol.All(cv.uint8_t, vol.Range(min=1, max=4)),
    vol.Required(CONF_CHANNEL): vol.All(cv.uint8_t, vol.Range(min=1, max=4)),
    vol.Required(CONF_STATE): cv.boolean,
    vol.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})
RC_SWITCH_TYPE_C_SCHEMA = vol.Schema({
    vol.Required(CONF_FAMILY): vol.All(
        cv.string, vol.Lower,
        cv.one_of('a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
                  'p')),
    vol.Required(CONF_GROUP): vol.All(cv.uint8_t, vol.Range(min=1, max=4)),
    vol.Required(CONF_DEVICE): vol.All(cv.uint8_t, vol.Range(min=1, max=4)),
    vol.Required(CONF_STATE): cv.boolean,
    vol.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})
RC_SWITCH_TYPE_D_SCHEMA = vol.Schema({
    vol.Required(CONF_GROUP): vol.All(cv.string, vol.Lower, cv.one_of('a', 'b', 'c', 'd')),
    vol.Required(CONF_DEVICE): vol.All(cv.uint8_t, vol.Range(min=1, max=3)),
    vol.Required(CONF_STATE): cv.boolean,
    vol.Optional(CONF_PROTOCOL, default=1): RC_SWITCH_PROTOCOL_SCHEMA,
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(RemoteTransmitterComponent),
    vol.Required(CONF_PIN): pins.gpio_output_pin_schema,
    vol.Optional(CONF_CARRIER_DUTY_PERCENT): vol.All(cv.percentage_int,
                                                     vol.Range(min=1, max=100)),
})])


def build_rc_switch_protocol(config):
    if isinstance(config, int):
        return rc_switch_protocols[config]
    pl = config[CONF_PULSE_LENGTH]
    return RCSwitchProtocol(config[CONF_SYNC][0] * pl, config[CONF_SYNC][1] * pl,
                            config[CONF_ZERO][0] * pl, config[CONF_ZERO][1] * pl,
                            config[CONF_ONE][0] * pl, config[CONF_ONE][1] * pl,
                            config[CONF_INVERTED])


def binary_code(value):
    code = 0
    for val in value:
        code <<= 1
        code |= val == '1'
    return HexInt(code)


def to_code(config):
    for conf in config:
        pin = None
        for pin in gpio_output_pin_expression(conf[CONF_PIN]):
            yield
        rhs = App.make_remote_transmitter_component(pin)
        transmitter = Pvariable(conf[CONF_ID], rhs)
        if CONF_CARRIER_DUTY_PERCENT in conf:
            add(transmitter.set_carrier_duty_percent(conf[CONF_CARRIER_DUTY_PERCENT]))


BUILD_FLAGS = '-DUSE_REMOTE_TRANSMITTER'
