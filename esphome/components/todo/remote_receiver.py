from esphome import pins
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_BUFFER_SIZE, CONF_DUMP, CONF_FILTER, CONF_ID, CONF_IDLE, \
    CONF_PIN, CONF_TOLERANCE

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
        raise cv.Invalid("Not valid dumpers")
    if value.upper() == "ALL":
        return list(sorted(list(DUMPERS)))
    raise cv.Invalid("Not valid dumpers")


CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(RemoteReceiverComponent),
    cv.Required(CONF_PIN): cv.All(pins.internal_gpio_input_pin_schema,
                                    pins.validate_has_interrupt),
    cv.Optional(CONF_DUMP, default=[]):
        cv.Any(validate_dumpers_all, cv.ensure_list(cv.one_of(*DUMPERS, lower=True))),
    cv.Optional(CONF_TOLERANCE): cv.All(cv.percentage_int, cv.Range(min=0)),
    cv.Optional(CONF_BUFFER_SIZE): cv.validate_bytes,
    cv.Optional(CONF_FILTER): cv.positive_time_period_microseconds,
    cv.Optional(CONF_IDLE): cv.positive_time_period_microseconds,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    pin = yield gpio_input_pin_expression(config[CONF_PIN])
    rhs = App.make_remote_receiver_component(pin)
    receiver = Pvariable(config[CONF_ID], rhs)

    for dumper in config[CONF_DUMP]:
        cg.add(receiver.add_dumper(DUMPERS[dumper].new()))
    if CONF_TOLERANCE in config:
        cg.add(receiver.set_tolerance(config[CONF_TOLERANCE]))
    if CONF_BUFFER_SIZE in config:
        cg.add(receiver.set_buffer_size(config[CONF_BUFFER_SIZE]))
    if CONF_FILTER in config:
        cg.add(receiver.set_filter_us(config[CONF_FILTER]))
    if CONF_IDLE in config:
        cg.add(receiver.set_idle_us(config[CONF_IDLE]))

    register_component(receiver, config)


BUILD_FLAGS = '-DUSE_REMOTE_RECEIVER'
