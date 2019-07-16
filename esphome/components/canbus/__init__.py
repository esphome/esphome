import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.automation import maybe_simple_id
from esphome.core import coroutine_with_priority

IS_PLATFORM_COMPONENT = True

CONF_CANBUS_ID = 'canbus_id'
CONF_CAN_ID = 'can_id'
CONF_CAN_DATA = 'can_data'

canbus_ns = cg.esphome_ns.namespace('canbus')
CanbusComponent = canbus_ns.class_('CanbusComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CanbusComponent),
}).extend(cv.COMPONENT_SCHEMA)

# Actions
SendAction = canbus_ns.class_('SendAction', automation.Action)

CANBUS_ACTION_SCHEMA = maybe_simple_id({
    cv.Required(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
    cv.Required(CONF_CAN_ID): cv.int_range(min=0, max=999),
    cv.Required(CONF_CAN_DATA): cv.All(),
})

@automation.register_action('canbus.send', SendAction, CANBUS_ACTION_SCHEMA)
def canbus_send_to_code(config, action_id, template_arg, args):
    canbus = yield cg.get_variable(config[CONF_CANBUS_ID])
    #paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, canbus, config[CONF_CAN_ID])

@coroutine_with_priority(100.0)
def to_code(config):
    cg.add_global(canbus_ns.using)
