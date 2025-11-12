import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .const import (
    CONF_ACK_TIMEOUT,
    CONF_CA_PEM,
    CONF_CLIENT_CRT,
    CONF_CLIENT_KEY,
    CONF_DEFAULT_MAX_BLOCK_SIZE,
    CONF_MAX_RETRANSMIT,
    CONF_OSCORE_CONF,
    CONF_PSK_IDENTITY,
    CONF_PSK_KEY,
    CONF_REQUEST_TIMEOUT,
)

CONF_COAP_CLIENT_ID = "coap_client_id"
CODEOWNERS = ["@rwrozelle"]

coap_client_component_ns = cg.esphome_ns.namespace("coap_client_component")
CoapClientComponent = coap_client_component_ns.class_(
    "CoapClientComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CoapClientComponent),
        cv.Optional(CONF_DEFAULT_MAX_BLOCK_SIZE): cv.uint16_t,
        cv.Optional(CONF_REQUEST_TIMEOUT): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_ACK_TIMEOUT): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_RETRANSMIT): cv.uint8_t,
        cv.Optional(CONF_OSCORE_CONF): cv.string,
        cv.Optional(CONF_PSK_IDENTITY): cv.string,
        cv.Optional(CONF_PSK_KEY): cv.string,
        cv.Optional(CONF_CA_PEM): cv.string,
        cv.Optional(CONF_CLIENT_CRT): cv.string,
        cv.Optional(CONF_CLIENT_KEY): cv.string,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_define("USE_COAP_CLIENT")
    add_idf_component(name="espressif/coap", ref="4.3.5~3")
    add_idf_sdkconfig_option("CONFIG_COAP_CLIENT_SUPPORT", True)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if (max_block_size := config.get(CONF_DEFAULT_MAX_BLOCK_SIZE)) is not None:
        cg.add(var.set_max_block_size(max_block_size))
    if (request_timeout := config.get(CONF_REQUEST_TIMEOUT)) is not None:
        cg.add(var.set_request_timeout(request_timeout))
    if (ack_timeout := config.get(CONF_ACK_TIMEOUT)) is not None:
        cg.add(var.set_ack_timeout(ack_timeout))
    if (max_retransmit := config.get(CONF_MAX_RETRANSMIT)) is not None:
        cg.add(var.set_max_retransmit(max_retransmit))
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
    if (client_crt := config.get(CONF_CLIENT_CRT)) is not None:
        cg.add(var.set_client_crt(client_crt))
    if (client_key := config.get(CONF_CLIENT_KEY)) is not None:
        cg.add(var.set_client_key(client_key))
