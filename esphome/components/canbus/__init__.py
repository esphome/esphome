import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.core import CORE, coroutine, coroutine_with_priority
from esphome.const import CONF_ID

IS_PLATFORM_COMPONENT = True

CONF_CANBUS_ID = 'canbus_id'
CONF_CAN_ID = 'can_id'
CONF_CAN_DATA = 'can_data'
CONF_SENDER_ID = 'sender_id'

canbus_ns = cg.esphome_ns.namespace('canbus')
CanbusComponent = canbus_ns.class_('CanbusComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CanbusComponent),
    cv.Required(CONF_SENDER_ID): cv.int_range(min=0, max=255),
}).extend(cv.COMPONENT_SCHEMA)

# Actions
SendAction = canbus_ns.class_('SendAction', automation.Action)

CANBUS_ACTION_SCHEMA = maybe_simple_id({
    cv.Required(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
    cv.Required(CONF_CAN_ID): cv.int_range(min=1, max=4096),
    cv.Required(CONF_CAN_DATA): cv.All(),
})


@coroutine
def setup_canbus_core_(var, config):
    yield cg.register_component(var, config)
    if CONF_CANBUS_ID in config:
        cg.add(var.set_canbus_id(config[CONF_CANBUS_ID]))
    if CONF_SENDER_ID in config:
        cg.add(var.set_sender_id([config[CONF_SENDER_ID]]))
    if CONF_CAN_DATA in config:
        cg.add(var.set_can_data([config[CONF_CAN_DATA]]))


@coroutine
def register_canbus(var, config):
    if not CORE.has_id(config[CONF_ID]):
        var = cg.Pvariable(config[CONF_ID], var)
    yield setup_canbus_core_(var, config)


@automation.register_action('canbus.send', SendAction, CANBUS_ACTION_SCHEMA)
def canbus_send_to_code(config, action_id, template_arg, args):
    canbus = yield cg.get_variable(config[CONF_CANBUS_ID])
    # paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, canbus, config[CONF_CAN_ID])


@coroutine_with_priority(100.0)
def to_code(config):
    cg.add_global(canbus_ns.using)
