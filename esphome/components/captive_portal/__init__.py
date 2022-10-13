import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import web_server_base
from esphome.components.web_server_base import CONF_WEB_SERVER_BASE_ID
from esphome.const import CONF_ID, CONF_NETWORKS, CONF_PASSWORD, CONF_SSID, CONF_WIFI
from esphome.core import coroutine_with_priority, CORE

AUTO_LOAD = ["web_server_base"]
DEPENDENCIES = ["wifi"]
CODEOWNERS = ["@OttoWinter"]

captive_portal_ns = cg.esphome_ns.namespace("captive_portal")
CaptivePortal = captive_portal_ns.class_("CaptivePortal", cg.Component)

CONF_KEEP_USER_CREDENTIALS = "keep_user_credentials"
CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CaptivePortal),
            cv.GenerateID(CONF_WEB_SERVER_BASE_ID): cv.use_id(
                web_server_base.WebServerBase
            ),
            cv.Optional(CONF_KEEP_USER_CREDENTIALS, default=False): cv.boolean,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_arduino,
)


def validate_wifi(config):
    wifi_conf = fv.full_config.get()[CONF_WIFI]
    if config.get(CONF_KEEP_USER_CREDENTIALS, False) and (
        CONF_SSID in wifi_conf
        or CONF_PASSWORD in wifi_conf
        or CONF_NETWORKS in wifi_conf
    ):
        raise cv.Invalid(
            f"WiFi credentials cannot be used together with {CONF_KEEP_USER_CREDENTIALS}"
        )
    return config


FINAL_VALIDATE_SCHEMA = validate_wifi


@coroutine_with_priority(64.0)
async def to_code(config):
    paren = await cg.get_variable(config[CONF_WEB_SERVER_BASE_ID])

    var = cg.new_Pvariable(config[CONF_ID], paren)
    await cg.register_component(var, config)
    cg.add_define("USE_CAPTIVE_PORTAL")

    if CORE.is_esp32:
        cg.add_library("DNSServer", None)
        cg.add_library("WiFi", None)
    if CORE.is_esp8266:
        cg.add_library("DNSServer", None)

    if config.get(CONF_KEEP_USER_CREDENTIALS, False):
        cg.add_define("USE_CAPTIVE_PORTAL_KEEP_USER_CREDENTIALS")
