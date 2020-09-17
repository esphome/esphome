import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.const as ehc
from esphome.core import coroutine_with_priority

from . import const as c
from . import cpp_types as t
from .aes_key import create_aes_key
from .md5sum import get_md5sum_hexint
from .peer import peer_to_code, PEER_SCHEMA
from .receive_trigger import validate_receive_trigger, receive_trigger_to_code

COMPONENT_SCHEMA = cv.All(cv.COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_id(t.Component),
    cv.Optional(ehc.CONF_CHANNEL): cv.All(cv.int_, cv.Range(min=1, max=14)),
    cv.Optional(ehc.CONF_PASSWORD): cv.All(cv.string, cv.Length(min=2)),
    cv.Optional(c.CONF_AESKEY): create_aes_key,
    cv.Required(c.CONF_PEERS): cv.ensure_list(PEER_SCHEMA),
    cv.Optional(c.CONF_ON_RECEIVE): validate_receive_trigger,
}), cv.has_at_most_one_key(ehc.CONF_PASSWORD, c.CONF_AESKEY))


@coroutine_with_priority(1.0)
def wifi_now_component_to_code(config):
    var = cg.new_Pvariable(config[ehc.CONF_ID])
    if ehc.CONF_CHANNEL in config:
        cg.add(var.set_channel(config[ehc.CONF_CHANNEL]))
    if ehc.CONF_PASSWORD in config:
        cg.add(var.set_aeskey(get_md5sum_hexint(config[ehc.CONF_PASSWORD])))
    if c.CONF_AESKEY in config:
        cg.add(var.set_aeskey( *config[c.CONF_AESKEY].as_hex_int()))
    for conf in config.get(c.CONF_PEERS, []):
        peer = yield peer_to_code(conf)
        cg.add(var.add_peer(peer))
    for conf in config.get(c.CONF_ON_RECEIVE, []):
        yield receive_trigger_to_code( var, conf)
    yield cg.register_component(var, config)
    yield var
