import urllib.parse as urlparse

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TIMEOUT,
    CONF_URL,
    CONF_METHOD,
    CONF_ESP8266_DISABLE_SSL_SUPPORT,
)
from esphome.components import esp32
from esphome.core import Lambda, CORE

CODEOWNERS = ["@oarcher"]

DEPENDENCIES = ["network"]
AUTO_LOAD = ["md5"]

ota_http_ns = cg.esphome_ns.namespace("ota_http")
OtaHttpComponent = ota_http_ns.class_("OtaHttpComponent", cg.Component)
OtaHttpArduino = ota_http_ns.class_("OtaHttpArduino", OtaHttpComponent)
OtaHttpIDF = ota_http_ns.class_("OtaHttpIDF", OtaHttpComponent)

OtaHttpFlashAction = ota_http_ns.class_("OtaHttpFlashAction", automation.Action)

CONF_VERIFY_SSL = "verify_ssl"


def validate_url(value):
    value = cv.string(value)
    try:
        parsed = list(urlparse.urlparse(value))
    except Exception as err:
        raise cv.Invalid("Invalid URL") from err

    if not parsed[0] or not parsed[1]:
        raise cv.Invalid("URL must have a URL scheme and host")

    if parsed[0] not in ["http", "https"]:
        raise cv.Invalid("Scheme must be http or https")

    if not parsed[2]:
        parsed[2] = "/"

    return urlparse.urlunparse(parsed)


def validate_secure_url(config):
    url_ = config[CONF_URL]
    if (
        config.get(CONF_VERIFY_SSL)
        and not isinstance(url_, Lambda)
        and url_.lower().startswith("https:")
    ):
        raise cv.Invalid(
            "Currently ESPHome doesn't support SSL verification. "
            "Set 'verify_ssl: false' to make insecure HTTPS requests."
        )
    return config


def _declare_request_class(value):
    if CORE.using_esp_idf:
        return cv.declare_id(OtaHttpIDF)(value)

    if CORE.is_esp8266 or CORE.is_esp32:
        return cv.declare_id(OtaHttpArduino)(value)
    return NotImplementedError


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {

            cv.GenerateID(): _declare_request_class,
            cv.Optional(
                CONF_TIMEOUT, default="5min"
            ): cv.positive_time_period_milliseconds,
            cv.SplitDefault(CONF_ESP8266_DISABLE_SSL_SUPPORT, esp8266=False): cv.All(
                cv.only_on_esp8266, cv.boolean
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 5, 1),
        esp32_arduino=cv.Version(0, 0, 0),
        esp_idf=cv.Version(0, 0, 0),
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))

    if CORE.is_esp8266 and not config[CONF_ESP8266_DISABLE_SSL_SUPPORT]:
        cg.add_define("USE_HTTP_REQUEST_ESP8266_HTTPS")

    if CORE.is_esp32:
        if CORE.using_esp_idf:
            esp32.add_idf_sdkconfig_option("CONFIG_ESP_TLS_INSECURE", True)
            esp32.add_idf_sdkconfig_option(
                "CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY", True
            )
        else:
            cg.add_library("WiFiClientSecure", None)
            cg.add_library("HTTPClient", None)
    if CORE.is_esp8266:
        cg.add_library("ESP8266HTTPClient", None)

    await cg.register_component(var, config)


OTA_HTTP_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(OtaHttpComponent),
        cv.Required(CONF_URL): cv.templatable(validate_url),
        cv.Optional(CONF_VERIFY_SSL, default=True): cv.boolean,
    }
).add_extra(validate_secure_url)
OTA_HTTP_FLASH_ACTION_SCHEMA = automation.maybe_conf(
    CONF_URL,
    OTA_HTTP_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_METHOD, default="flash"): cv.string,
        }
    ),
)


@automation.register_action(
    "ota_http.flash", OtaHttpFlashAction, OTA_HTTP_FLASH_ACTION_SCHEMA
)
async def ota_http_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(template_))
    return var
