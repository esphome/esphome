from esphome import automation
import esphome.codegen as cg
import esphome.const as ehc
import esphome.config_validation as cv
from esphome.core import CORE, coroutine

from . import const as c
from . import cpp_types as t
from .service_key import create_service_key
from .md5sum import get_md5sum_hexint
from .payload_getter import build_payload_getter_list, validate_payload_getter_list

SEND_SCHEMA = cv.Schema({
    cv.Optional(c.CONF_PEERID): cv.use_id(t.Peer),
    cv.Optional(ehc.CONF_SERVICE): cv.All(cv.string, cv.Length(min=2)),
    cv.Optional(c.CONF_SERVICEKEY): create_service_key,
    cv.Optional(c.CONF_PAYLOADS): validate_payload_getter_list,
    cv.Optional(c.CONF_ON_FAIL): automation.validate_action_list,
    cv.Optional(c.CONF_ON_SUCCESS): automation.validate_action_list,
    })


@automation.register_action(c.ACTION_SEND, t.SendAction, SEND_SCHEMA)
@coroutine
def send_action_to_code(config, action_id, template_arg, args):
    component = yield cg.get_variable(CORE.config[c.CONF_WIFI_NOW][ehc.CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, component)
    if c.CONF_PEERID in config:
        peer = yield cg.get_variable(config[c.CONF_PEERID])
        cg.add(var.set_peer(peer))
    if ehc.CONF_SERVICE in config:
        cg.add(var.set_servicekey(get_md5sum_hexint(config[ehc.CONF_SERVICE], 7)))
    if c.CONF_SERVICEKEY in config:
        cg.add(var.set_servicekey(*config[c.CONF_SERVICEKEY].to_hex_int()))
    if config.get(c.CONF_PAYLOADS):
        payload_getters = yield build_payload_getter_list(
            config[c.CONF_PAYLOADS],
            template_arg, args
            )
        cg.add(var.set_payload_getters(payload_getters))
    ActionPtr = t.SendAction.template(template_arg).operator('ptr')
    args = args + [(ActionPtr, 'sendaction')]
    template_arg = cg.TemplateArguments(*[arg[0] for arg in args])
    if c.CONF_ON_FAIL in config:
        actions = yield automation.build_action_list(config[c.CONF_ON_FAIL], template_arg, args)
        cg.add(var.add_on_fail(actions))
    if c.CONF_ON_SUCCESS in config:
        actions = yield automation.build_action_list(config[c.CONF_ON_SUCCESS], template_arg, args)
        cg.add(var.add_on_success(actions))
    yield var
