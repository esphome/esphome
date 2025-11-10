import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component, add_idf_sdkconfig_option
import esphome.config_validation as cv
from esphome.const import CONF_ID

CONF_COAP_CLIENT_ID = "coap_client_id"
CODEOWNERS = ["@rwrozelle"]

coap_client_component_ns = cg.esphome_ns.namespace("coap_client_component")
CoapClientComponent = coap_client_component_ns.class_(
    "CoapClientComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CoapClientComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_define("USE_COAP_CLIENT")
    add_idf_component(name="espressif/coap", ref="4.3.5~3")
    add_idf_sdkconfig_option("CONFIG_COAP_CLIENT_SUPPORT", True)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
