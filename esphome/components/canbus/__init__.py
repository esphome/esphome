import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.automation import Condition, maybe_simple_id
from esphome.components import binary_sensor
from esphome import automation
from esphome import pins
from esphome.const import CONF_BUFFER_SIZE, CONF_DUMP, CONF_FILTER, CONF_ID, CONF_IDLE, \
    CONF_PIN, CONF_TOLERANCE

canbus_ns = cg.esphome_ns.namespace('canbus')
CanbusComponent = canbus_ns.class_('CanbusComponent', cg.Component)

IS_PLATFORM_COMPONENT = True

CONF_CANBUS_ID = 'canbus_id'
CONF_CAN_ID = 'can_id'
CONF_DATA = 'data'

SendAction = canbus_ns.class_('SendAction', automation.Action)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(CanbusComponent),
    cv.SplitDefault(CONF_BUFFER_SIZE, esp32='10000b', esp8266='1000b'): cv.validate_bytes,
}).extend(cv.COMPONENT_SCHEMA)

CANBUS_ACTION_SCHEMA = maybe_simple_id({
    cv.Required(CONF_ID): cv.use_id(binary_sensor),
})

@automation.register_action('canbus.send', SendAction, CANBUS_ACTION_SCHEMA)
def canbus_send_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    yield cg.new_Pvariable(action_id, template_arg, paren)

def to_code(config):
    print("canbus to_code")
    #var = cg.new_Pvariable(config[CONF_ID])
    # yield cg.register_component(var, config)
    # cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
