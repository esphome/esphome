import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.const as ehc
from esphome.core import coroutine, HexInt
from esphome.util import Registry

from . import const as c
from . import cpp_types as t
from .aes_key import AesKey, create_aes_key
from .md5sum import get_md5sum_hexint

# WifiNowPeer Object Validation and Code Geneneration
PEER_SCHEMA = cv.All(cv.Schema( {
    cv.GenerateID(): cv.declare_id(t.Peer),
    cv.Required(ehc.CONF_BSSID): cv.mac_address,
    cv.Optional(ehc.CONF_PASSWORD): cv.All( cv.string, cv.Length(min=2)),
    cv.Optional(c.CONF_AESKEY): create_aes_key,
}), cv.has_at_most_one_key(ehc.CONF_PASSWORD, c.CONF_AESKEY))

@coroutine
def peer_to_code(config):
    var = cg.new_Pvariable(config[ehc.CONF_ID])
    cg.add(var.set_bssid([HexInt(i) for i in config[ehc.CONF_BSSID].parts]))
    if ehc.CONF_PASSWORD in config:
        cg.add(var.set_aeskey(get_md5sum_hexint(config[ehc.CONF_PASSWORD])))
    if c.CONF_AESKEY in config:
        cg.add(var.set_aeskey( *config[c.CONF_AESKEY].as_hex_int()))
    yield var
