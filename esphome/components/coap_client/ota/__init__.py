from esphome import automation
import esphome.codegen as cg
from esphome.components.esp32 import VARIANT_ESP32C6, VARIANT_ESP32H2, only_on_variant
from esphome.components.ota import BASE_OTA_SCHEMA, OTAComponent, ota_to_code
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_URL
from esphome.core import coroutine_with_priority
from esphome.coroutine import CoroPriority

from .. import CONF_COAP_CLIENT_ID, CoapClientComponent, coap_client_component_ns

CODEOWNERS = ["@rwrozelle"]

AUTO_LOAD = ["md5", "watchdog"]
DEPENDENCIES = ["network", "coap_client"]

CONF_MD5 = "md5"
CONF_MD5_URL = "md5_url"

OtaCoapClientComponent = coap_client_component_ns.class_(
    "OtaCoapClientComponent", OTAComponent
)
OtaCoapClientComponentFlashAction = coap_client_component_ns.class_(
    "OtaCoapClientComponentFlashAction", automation.Action
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(OtaCoapClientComponent),
            cv.GenerateID(CONF_COAP_CLIENT_ID): cv.use_id(CoapClientComponent),
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp_idf=cv.Version(0, 0, 0),
    ),
)


@coroutine_with_priority(CoroPriority.OTA_UPDATES)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_COAP_CLIENT_ID])


OTA_COAP_CLIENT_FLASH_ACTION_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(OtaCoapClientComponent),
            cv.Optional(CONF_MD5_URL): cv.templatable(cv.url),
            cv.Optional(CONF_MD5): cv.templatable(
                cv.All(cv.string, cv.Length(min=32, max=32))
            ),
            cv.Required(CONF_URL): cv.templatable(cv.url),
        }
    ),
    cv.has_exactly_one_key(CONF_MD5, CONF_MD5_URL),
    only_on_variant(supported=[VARIANT_ESP32C6, VARIANT_ESP32H2]),
)


@automation.register_action(
    "ota.coap_client.flash",
    OtaCoapClientComponentFlashAction,
    OTA_COAP_CLIENT_FLASH_ACTION_SCHEMA,
)
async def ota_coap_client_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    if md5_url := config.get(CONF_MD5_URL):
        template_ = await cg.templatable(md5_url, args, cg.std_string)
        cg.add(var.set_md5_url(template_))

    if md5_str := config.get(CONF_MD5):
        template_ = await cg.templatable(md5_str, args, cg.std_string)
        cg.add(var.set_md5(template_))

    template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(template_))

    return var
