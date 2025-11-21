import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .const import (
    CONF_ACK_TIMEOUT,
    CONF_MAX_BLOCK_SIZE,
    CONF_MAX_RETRANSMIT,
    CONF_REQUEST_TIMEOUT,
)

CODEOWNERS = ["@rwrozelle"]

DEPENDENCIES = ["network"]
AUTO_LOAD = ["json", "watchdog"]

coap_client_component_ns = cg.esphome_ns.namespace("coap_client")
CoapClientComponent = coap_client_component_ns.class_(
    "CoapClientComponent", cg.Component
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CoapClientComponent),
            cv.Optional(CONF_MAX_BLOCK_SIZE, default="512B"): cv.validate_bytes,
            cv.Optional(
                CONF_REQUEST_TIMEOUT, default="2sec"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(
                CONF_ACK_TIMEOUT, default="2sec"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_RETRANSMIT, default=4): cv.uint8_t,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def to_code(config):
    cg.add_define("USE_COAP_CLIENT")
    add_idf_component(name="espressif/coap", ref="4.3.5~3")
    add_idf_sdkconfig_option("CONFIG_COAP_CLIENT_SUPPORT", True)
    add_idf_sdkconfig_option("CONFIG_COAP_TCP_SUPPORT", False)
    add_idf_sdkconfig_option("CONFIG_COAP_MBEDTLS_PSK", False)
    add_idf_sdkconfig_option("CONFIG_COAP_ASYNC_SUPPORT", False)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    if (max_block_size := config.get(CONF_MAX_BLOCK_SIZE)) is not None:
        cg.add(var.set_max_block_size(max_block_size))
    if (request_timeout := config.get(CONF_REQUEST_TIMEOUT)) is not None:
        cg.add(var.set_request_timeout(request_timeout))
    if (ack_timeout := config.get(CONF_ACK_TIMEOUT)) is not None:
        cg.add(var.set_ack_timeout(ack_timeout))
    if (max_retransmit := config.get(CONF_MAX_RETRANSMIT)) is not None:
        cg.add(var.set_max_retransmit(max_retransmit))
