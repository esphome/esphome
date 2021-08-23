import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import switch
from esphome.const import CONF_ID, CONF_KEY
from .. import CONF_API_CLIENT_ID, APIClient

DEPENDENCIES = ["api_client"]

api_client_ns = cg.esphome_ns.namespace("api_client")
ApiSwitch = api_client_ns.class_("ApiClientSwitch", switch.Switch, cg.Component)

CONFIG_SCHEMA = switch.SWITCH_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ApiSwitch),
        cv.GenerateID(CONF_API_CLIENT_ID): cv.use_id(APIClient),
        cv.Required(CONF_KEY): cv.uint32_t,
    }
).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    paren = yield cg.get_variable(config[CONF_API_CLIENT_ID])
    var = cg.new_Pvariable(config[CONF_ID])

    yield switch.register_switch(var, config)

    cg.add(var.set_api_key(config[CONF_KEY]))
    cg.add(var.set_api_parent(paren))
    cg.add(paren.register_switch(config[CONF_KEY], var))
