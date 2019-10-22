import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.core import CORE, coroutine, coroutine_with_priority
from esphome.py_compat import text_type, binary_type, char_to_byte
from esphome.const import CONF_ID, CONF_TRIGGER_ID, CONF_DATA

IS_PLATFORM_COMPONENT = True

CONF_CANBUS_ID = 'canbus_id'
CONF_CAN_ID = 'can_id'
CONF_SENDER_ID = 'sender_id'
CONF_BIT_RATE = 'bit_rate'
CONF_ON_FRAME = 'on_frame'

CONF_CANBUS_SEND_ACTION = 'canbus.send'


def validate_raw_data(value):
    if isinstance(value, text_type):
        return value.encode('utf-8')
    if isinstance(value, str):
        return value
    if isinstance(value, list):
        return cv.Schema([cv.hex_uint8_t])(value)
    raise cv.Invalid("data must either be a string wrapped in quotes or a list of bytes")


canbus_ns = cg.esphome_ns.namespace('canbus')
CanbusComponent = canbus_ns.class_('CanbusComponent', cg.Component)
CanbusTrigger = canbus_ns.class_('CanbusTrigger',
                                 automation.Trigger.template(cg.std_vector.template(cg.uint8)),
                                 cg.Component)
CanSpeed = canbus_ns.enum('CAN_SPEED')

CAN_SPEEDS = {
    '5KBPS': CanSpeed.CAN_5KBPS,
    '10KBPS': CanSpeed.CAN_10KBPS,
    '20KBPS': CanSpeed.CAN_20KBPS,
    '31K25BPS': CanSpeed.CAN_31K25BPS,
    '33KBPS': CanSpeed.CAN_33KBPS,
    '40KBPS': CanSpeed.CAN_40KBPS,
    '50KBPS': CanSpeed.CAN_50KBPS,
    '80KBPS': CanSpeed.CAN_80KBPS,
    '83K3BPS': CanSpeed.CAN_83K3BPS,
    '95KBPS': CanSpeed.CAN_95KBPS,
    '100KBPS': CanSpeed.CAN_100KBPS,
    '125KBPS': CanSpeed.CAN_125KBPS,
    '200KBPS': CanSpeed.CAN_200KBPS,
    '250KBPS': CanSpeed.CAN_250KBPS,
    '500KBPS': CanSpeed.CAN_500KBPS,
    '1000KBPS': CanSpeed.CAN_1000KBPS,
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CanbusComponent),
    cv.Required(CONF_SENDER_ID): cv.int_range(min=0, max=255),
    cv.Optional(CONF_BIT_RATE, default='125KBPS'): cv.enum(CAN_SPEEDS, upper=True),
    cv.Optional(CONF_ON_FRAME): automation.validate_automation({
        cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(CanbusTrigger),
        cv.GenerateID(CONF_CAN_ID): cv.int_range(min=1, max=4096),
    }),
}).extend(cv.COMPONENT_SCHEMA)

# Actions
CanbusSendAction = canbus_ns.class_('CanbusSendAction', automation.Action)

CANBUS_ACTION_SCHEMA = cv.Schema({
    cv.Required(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
    cv.Required(CONF_CAN_ID): cv.int_range(min=1, max=4096),
    cv.Required(CONF_DATA): cv.templatable(validate_raw_data),
})


@coroutine
def setup_canbus_core_(var, config):
    yield cg.register_component(var, config)
    if CONF_SENDER_ID in config:
        cg.add(var.set_sender_id([config[CONF_SENDER_ID]]))
    if CONF_BIT_RATE in config:
        bitrate = CAN_SPEEDS[config[CONF_BIT_RATE]]
        cg.add(var.set_bitrate(bitrate))
    for conf in config.get(CONF_ON_FRAME, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var, conf[CONF_CAN_ID])
        yield cg.register_component(trigger, conf)
        yield automation.build_automation(trigger, [(cg.std_vector.template(cg.uint8), 'x')], conf)


@coroutine
def register_canbus(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.new_Pvariable(config[CONF_ID], var)
    yield setup_canbus_core_(var, config)


@automation.register_action(CONF_CANBUS_SEND_ACTION, CanbusSendAction, CANBUS_ACTION_SCHEMA)
def canbus_action_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    yield cg.register_parented(var, config[CONF_CANBUS_ID])

    can_id = yield cg.templatable(config[CONF_CAN_ID], args, cg.uint16)
    cg.add(var.set_can_id(can_id))
    data = config[CONF_DATA]
    if isinstance(data, binary_type):
        data = [char_to_byte(x) for x in data]
    if cg.is_template(data):
        templ = yield cg.templatable(data, args, cg.std_vector.template(cg.uint8))
        cg.add(var.set_data_template(templ))
    else:
        cg.add(var.set_data_static(data))
    yield var


@coroutine_with_priority(100.0)
def to_code(config):
    cg.add_global(canbus_ns.using)
    cg.add_define("USE_CANBUS")
