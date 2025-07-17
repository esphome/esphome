import esphome.codegen as cg
from esphome.components.esp32 import add_idf_component
from esphome.components.ota import BASE_OTA_SCHEMA, OTAComponent, ota_to_code
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE, coroutine_with_priority

CODEOWNERS = ["@esphome/core"]
DEPENDENCIES = ["network", "web_server_base"]

web_server_ns = cg.esphome_ns.namespace("web_server")
WebServerOTAComponent = web_server_ns.class_("WebServerOTAComponent", OTAComponent)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(WebServerOTAComponent),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


@coroutine_with_priority(52.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    await cg.register_component(var, config)
    cg.add_define("USE_WEBSERVER_OTA")
    if CORE.using_esp_idf:
        add_idf_component(name="zorxx/multipart-parser", ref="1.0.1")
