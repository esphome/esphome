import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .const import (
    CONF_CA_PEM,
    CONF_KEEP_ALIVE,
    CONF_LISTEN_PORT,
    CONF_MAX_IDLE_SESSIONS,
    CONF_MCAST_IP_MODE_V4_ADDR,
    CONF_MCAST_IP_MODE_V6_ADDR,
    CONF_OSCORE_CONF,
    CONF_PSK_IDENTITY,
    CONF_PSK_KEY,
    CONF_SECURE_LISTEN_PORT,
    CONF_SECURE_WEBSOCKET_PORT,
    CONF_SERVER_CRT,
    CONF_SERVER_KEY,
    CONF_WEBSOCKET_PORT,
)

CONF_COAP_SERVER_ID = "coap_server_id"
CODEOWNERS = ["@rwrozelle"]

coap_server_component_ns = cg.esphome_ns.namespace("coap")
CoapServerComponent = coap_server_component_ns.class_(
    "CoapServerComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CoapServerComponent),
        cv.Optional(CONF_LISTEN_PORT, default="5683"): cv.uint16_t,
        cv.Optional(CONF_SECURE_LISTEN_PORT, default="5684"): cv.uint16_t,
        cv.Optional(CONF_WEBSOCKET_PORT, default="80"): cv.uint16_t,
        cv.Optional(CONF_SECURE_WEBSOCKET_PORT, default="443"): cv.uint16_t,
        cv.Optional(CONF_MCAST_IP_MODE_V4_ADDR, default="224.0.1.187"): cv.string,
        cv.Optional(CONF_MCAST_IP_MODE_V4_ADDR, default="FF02::FD"): cv.string,
        cv.Optional(CONF_MAX_IDLE_SESSIONS, default="20"): cv.uint16_t,
        cv.Optional(CONF_KEEP_ALIVE, default="30"): cv.uint16_t,
        cv.Optional(CONF_OSCORE_CONF): cv.string,
        cv.Optional(CONF_PSK_IDENTITY): cv.string,
        cv.Optional(CONF_PSK_KEY): cv.string,
        cv.Optional(CONF_CA_PEM): cv.string,
        cv.Optional(CONF_SERVER_CRT): cv.string,
        cv.Optional(CONF_SERVER_KEY): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_define("USE_COAP_SERVER")
    add_idf_component(name="espressif/coap", ref="4.3.5~3")
    add_idf_sdkconfig_option("CONFIG_COAP_SERVER_SUPPORT", True)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if (listen_port := config.get(CONF_LISTEN_PORT)) is not None:
        cg.add(var.set_listen_port(listen_port))
    if (secure_listen_port := config.get(CONF_SECURE_LISTEN_PORT)) is not None:
        cg.add(var.set_secure_listen_port(secure_listen_port))
    if (websocket_port := config.get(CONF_WEBSOCKET_PORT)) is not None:
        cg.add(var.set_websocket_port(websocket_port))
    if (secure_websocket_port := config.get(CONF_SECURE_WEBSOCKET_PORT)) is not None:
        cg.add(var.set_secure_websocket_port(secure_websocket_port))
    if (mcast_ip_mode_v4_addr := config.get(CONF_MCAST_IP_MODE_V4_ADDR)) is not None:
        cg.add(var.set_mcast_ip_mode_v4_addr(mcast_ip_mode_v4_addr))
    if (mcast_ip_mode_v6_addr := config.get(CONF_MCAST_IP_MODE_V6_ADDR)) is not None:
        cg.add(var.set_mcast_ip_mode_v6_addr(mcast_ip_mode_v6_addr))
    if (idle_sessions := config.get(CONF_MAX_IDLE_SESSIONS)) is not None:
        cg.add(var.set_max_idle_sessions(idle_sessions))
    if (keep_alive := config.get(CONF_KEEP_ALIVE)) is not None:
        cg.add(var.set_keep_alive(keep_alive))
    if (oscore_conf := config.get(CONF_OSCORE_CONF)) is not None:
        add_idf_sdkconfig_option("CONFIG_COAP_OSCORE_SUPPORT", True)
        cg.add(var.set_oscore_conf(oscore_conf))
    if (psk_identity := config.get(CONF_PSK_IDENTITY)) is not None:
        add_idf_sdkconfig_option("CONFIG_COAP_MBEDTLS_PSK", True)
        cg.add(var.set_psk_identity(psk_identity))
    if (psk_key := config.get(CONF_PSK_KEY)) is not None:
        cg.add(var.set_psk_key(psk_key))
    if (ca_pem := config.get(CONF_CA_PEM)) is not None:
        add_idf_sdkconfig_option("CONFIG_COAP_MBEDTLS_PKI", True)
        cg.add(var.set_psk_key(ca_pem))
    if (server_crt := config.get(CONF_SERVER_CRT)) is not None:
        cg.add(var.set_server_crt(server_crt))
    if (server_key := config.get(CONF_SERVER_KEY)) is not None:
        cg.add(var.set_server_key(server_key))
