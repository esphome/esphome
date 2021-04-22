import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID

ddns_ns = cg.esphome_ns.namespace("ddns")

DOMAIN_KEY = 'domain'
USERNAME_KEY = 'username'
PASSWORD_KEY = 'password'
SERVICE_KEY = 'service'
TOKEN_KEY = 'token'
UPDATE_INTERVAL_KEY = 'update_interval'
USE_LOCAL_IP_KEY = 'use_local_ip'
DDNS = ddns_ns.class_("DDNSComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(DDNS),
    cv.Required(SERVICE_KEY): cv.string,
    cv.Required(DOMAIN_KEY): cv.string,
    cv.Optional(USERNAME_KEY): cv.string,
    cv.Optional(PASSWORD_KEY): cv.string,
    cv.Optional(TOKEN_KEY): cv.string,
    cv.Optional(UPDATE_INTERVAL_KEY, default=10000): cv.int_,
    cv.Optional(USE_LOCAL_IP_KEY, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    # cg.add_library("EasyDDNS", "1.6.0")
    # cg.add_library("WiFiClientSecure", None)
    # cg.add_library("HTTPClient", None)

    cg.add(var.set_service(config[SERVICE_KEY]))
    if TOKEN_KEY in config:
        cg.add(var.set_client(config[DOMAIN_KEY], config[TOKEN_KEY]))
    else:
        cg.add(var.set_client(config[DOMAIN_KEY], config[USERNAME_KEY], config[PASSWORD_KEY]))

    cg.add(var.set_update(config[UPDATE_INTERVAL_KEY], config[USE_LOCAL_IP_KEY]))
