import esphome.codegen as cg
from esphome.components import update
import esphome.config_validation as cv
from esphome.const import CONF_SOURCE

from .. import CONF_COAP_CLIENT_ID, CoapClientComponent, coap_client_ns
from ..ota import OtaCoapClientComponent

AUTO_LOAD = ["json"]
CODEOWNERS = ["@rwrozelle"]
DEPENDENCIES = ["ota.coap_client"]

CoapClientUpdate = coap_client_ns.class_(
    "CoapClientUpdate", update.UpdateEntity, cg.PollingComponent
)

CONF_OTA_ID = "ota_id"

CONFIG_SCHEMA = (
    update.update_schema(CoapClientUpdate)
    .extend(
        {
            cv.GenerateID(CONF_OTA_ID): cv.use_id(OtaCoapClientComponent),
            cv.GenerateID(CONF_COAP_CLIENT_ID): cv.use_id(CoapClientComponent),
            cv.Required(CONF_SOURCE): cv.url,
        }
    )
    .extend(cv.polling_component_schema("6h"))
)


async def to_code(config):
    var = await update.new_update(config)
    ota_parent = await cg.get_variable(config[CONF_OTA_ID])
    cg.add(var.set_ota_parent(ota_parent))
    request_parent = await cg.get_variable(config[CONF_COAP_CLIENT_ID])
    cg.add(var.set_request_parent(request_parent))

    cg.add(var.set_source_url(config[CONF_SOURCE]))

    cg.add_define("USE_OTA_STATE_CALLBACK")

    await cg.register_component(var, config)
