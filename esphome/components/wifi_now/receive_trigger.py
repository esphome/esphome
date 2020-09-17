import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import coroutine
from esphome import automation

from . import const as c
from . import cpp_types as t
from .service_key import create_service_key
from .md5sum import get_md5sum_hexint
from .payload_setter import validate_payload_setter_list, build_payload_setter_list


validate_receive_trigger = automation.validate_automation( {
        cv.GenerateID(c.CONF_TRIGGER_ID): cv.declare_id(t.ReceiveTrigger),
        cv.Optional(c.CONF_PEERID): cv.use_id(t.Peer),
        cv.Optional(c.CONF_SERVICE): cv.All( cv.string, cv.Length(min=2)),
        cv.Optional(c.CONF_SERVICEKEY): create_service_key,
        cv.Required(c.CONF_PAYLOADS): validate_payload_setter_list,
    }, cv.has_at_most_one_key(c.CONF_SERVICE, c.CONF_SERVICEKEY))

@coroutine
def receive_trigger_to_code( component, config):
    var = cg.new_Pvariable(config[c.CONF_TRIGGER_ID], component)
    if c.CONF_PEERID in config:
        peer = yield cg.get_variable(config[c.CONF_PEERID])
        cg.add(var.set_peer(peer))
    if c.CONF_SERVICE in config:
        cg.add(var.set_servicekey(get_md5sum_hexint(config[c.CONF_SERVICE], 7)))
    if c.CONF_SERVICEKEY in config:
        cg.add(var.set_servicekey(*config[c.CONF_SERVICEKEY].to_hex_int()))
    payload_setters = yield build_payload_setter_list(config[c.CONF_PAYLOADS], cg.TemplateArguments([]), [])
    cg.add(var.set_payload_setters(payload_setters))
    yield automation.build_automation(var, [], config)
    yield cg.register_component(var, config)
    yield var
