import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.const import CONF_BUFFER_SIZE, CONF_DUMP, CONF_FILTER, CONF_ID, CONF_IDLE, \
    CONF_PIN, CONF_TOLERANCE
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns, gpio_input_pin_expression, \
    setup_component, Component

remote_ns = esphomelib_ns.namespace('remote')

RemoteControlComponentBase = remote_ns.class_('RemoteControlComponentBase')
RemoteReceiverComponent = remote_ns.class_('RemoteReceiverComponent',
                                           RemoteControlComponentBase,
                                           Component)

RemoteReceiveDumper = remote_ns.class_('RemoteReceiveDumper')

DUMPERS = {
    'lg': remote_ns.class_('LGDumper', RemoteReceiveDumper),
    'nec': remote_ns.class_('NECDumper', RemoteReceiveDumper),
    'panasonic': remote_ns.class_('PanasonicDumper', RemoteReceiveDumper),
    'raw': remote_ns.class_('RawDumper', RemoteReceiveDumper),
    'samsung': remote_ns.class_('SamsungDumper', RemoteReceiveDumper),
    'sony': remote_ns.class_('SonyDumper', RemoteReceiveDumper),
    'rc_switch': remote_ns.class_('RCSwitchDumper', RemoteReceiveDumper),
}


def validate_dumpers_all(value):
    if not isinstance(value, (str, unicode)):
        raise vol.Invalid("Not valid dumpers")
    if value.upper() == "ALL":
        return list(sorted(list(DUMPERS)))
    raise vol.Invalid("Not valid dumpers")


CONFIG_SCHEMA = vol.All(cv.ensure_list, [vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(RemoteReceiverComponent),
    vol.Required(CONF_PIN): pins.gpio_input_pin_schema,
    vol.Optional(CONF_DUMP, default=[]):
        vol.Any(validate_dumpers_all,
                vol.All(cv.ensure_list, [vol.All(vol.Lower, cv.one_of(*DUMPERS))])),
    vol.Optional(CONF_TOLERANCE): vol.All(cv.percentage_int, vol.Range(min=0)),
    vol.Optional(CONF_BUFFER_SIZE): cv.validate_bytes,
    vol.Optional(CONF_FILTER): cv.positive_time_period_microseconds,
    vol.Optional(CONF_IDLE): cv.positive_time_period_microseconds,
}).extend(cv.COMPONENT_SCHEMA.schema)])


def to_code(config):
    for conf in config:
        for pin in gpio_input_pin_expression(conf[CONF_PIN]):
            yield
        rhs = App.make_remote_receiver_component(pin)
        receiver = Pvariable(conf[CONF_ID], rhs)

        for dumper in conf[CONF_DUMP]:
            add(receiver.add_dumper(DUMPERS[dumper].new()))
        if CONF_TOLERANCE in conf:
            add(receiver.set_tolerance(conf[CONF_TOLERANCE]))
        if CONF_BUFFER_SIZE in conf:
            add(receiver.set_buffer_size(conf[CONF_BUFFER_SIZE]))
        if CONF_FILTER in conf:
            add(receiver.set_filter_us(conf[CONF_FILTER]))
        if CONF_IDLE in conf:
            add(receiver.set_idle_us(conf[CONF_IDLE]))

        setup_component(receiver, conf)


BUILD_FLAGS = '-DUSE_REMOTE_RECEIVER'
