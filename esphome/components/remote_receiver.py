import voluptuous as vol

from esphome import pins
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_DUMP, CONF_FILTER, CONF_ID, CONF_IDLE, \
    CONF_PIN, CONF_TOLERANCE
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_input_pin_expression, setup_component
from esphome.cpp_types import App, Component, esphome_ns
from esphome.py_compat import string_types

remote_ns = esphome_ns.namespace('remote')
MULTI_CONF = True

RemoteControlComponentBase = remote_ns.class_('RemoteControlComponentBase')
RemoteReceiverComponent = remote_ns.class_('RemoteReceiverComponent',
                                           RemoteControlComponentBase,
                                           Component)

RemoteReceiveDumper = remote_ns.class_('RemoteReceiveDumper')

DUMPERS = {
    'jvc': remote_ns.class_('JVCDumper', RemoteReceiveDumper),
    'lg': remote_ns.class_('LGDumper', RemoteReceiveDumper),
    'nec': remote_ns.class_('NECDumper', RemoteReceiveDumper),
    'panasonic': remote_ns.class_('PanasonicDumper', RemoteReceiveDumper),
    'raw': remote_ns.class_('RawDumper', RemoteReceiveDumper),
    'samsung': remote_ns.class_('SamsungDumper', RemoteReceiveDumper),
    'sony': remote_ns.class_('SonyDumper', RemoteReceiveDumper),
    'rc_switch': remote_ns.class_('RCSwitchDumper', RemoteReceiveDumper),
    'rc5': remote_ns.class_('RC5Dumper', RemoteReceiveDumper),
}


def validate_dumpers_all(value):
    if not isinstance(value, string_types):
        raise vol.Invalid("Not valid dumpers")
    if value.upper() == "ALL":
        return list(sorted(list(DUMPERS)))
    raise vol.Invalid("Not valid dumpers")


CONFIG_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(RemoteReceiverComponent),
    vol.Required(CONF_PIN): pins.gpio_input_pin_schema,
    vol.Optional(CONF_DUMP, default=[]):
        vol.Any(validate_dumpers_all, cv.ensure_list(cv.one_of(*DUMPERS, lower=True))),
    vol.Optional(CONF_TOLERANCE): vol.All(cv.percentage_int, vol.Range(min=0)),
    vol.Optional(CONF_BUFFER_SIZE): cv.validate_bytes,
    vol.Optional(CONF_FILTER): cv.positive_time_period_microseconds,
    vol.Optional(CONF_IDLE): cv.positive_time_period_microseconds,
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    for pin in gpio_input_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_remote_receiver_component(pin)
    receiver = Pvariable(config[CONF_ID], rhs)

    for dumper in config[CONF_DUMP]:
        add(receiver.add_dumper(DUMPERS[dumper].new()))
    if CONF_TOLERANCE in config:
        add(receiver.set_tolerance(config[CONF_TOLERANCE]))
    if CONF_BUFFER_SIZE in config:
        add(receiver.set_buffer_size(config[CONF_BUFFER_SIZE]))
    if CONF_FILTER in config:
        add(receiver.set_filter_us(config[CONF_FILTER]))
    if CONF_IDLE in config:
        add(receiver.set_idle_us(config[CONF_IDLE]))

    setup_component(receiver, config)


BUILD_FLAGS = '-DUSE_REMOTE_RECEIVER'
