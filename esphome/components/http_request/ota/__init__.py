import urllib.parse as urlparse
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome import automation
from esphome.const import (
    CONF_ID,
    CONF_TIMEOUT,
    CONF_URL,
    CONF_METHOD,
    CONF_ESP8266_DISABLE_SSL_SUPPORT,
    CONF_SAFE_MODE,
    CONF_FORCE_UPDATE,
)
from esphome.components import esp32
from esphome.components.ota import BASE_OTA_SCHEMA, ota_to_code, OTAComponent
from esphome.core import CORE, coroutine_with_priority
from .. import http_request_ns

CODEOWNERS = ["@oarcher"]

DEPENDENCIES = ["network"]
AUTO_LOAD = ["md5", "ota"]

CONF_EXCLUDE_CERTIFICATE_BUNDLE = "exclude_certificate_bundle"
CONF_MAX_URL_LENGTH = "max_url_length"
CONF_MD5_URL = "md5_url"
CONF_WATCHDOG_TIMEOUT = "watchdog_timeout"

OtaHttpRequestComponent = http_request_ns.class_(
    "OtaHttpRequestComponent", OTAComponent
)
OtaHttpRequestComponentArduino = http_request_ns.class_(
    "OtaHttpRequestComponentArduino", OtaHttpRequestComponent
)
OtaHttpRequestComponentIDF = http_request_ns.class_(
    "OtaHttpRequestComponentIDF", OtaHttpRequestComponent
)
OtaHttpRequestComponentFlashAction = http_request_ns.class_(
    "OtaHttpRequestComponentFlashAction", automation.Action
)


def validate_certificate_bundle(config):
    if not (CORE.is_esp8266 and config.get(CONF_ESP8266_DISABLE_SSL_SUPPORT)) and (
        not config.get(CONF_EXCLUDE_CERTIFICATE_BUNDLE) and not CORE.using_esp_idf
    ):
        raise cv.Invalid(
            "ESPHome supports certificate verification only via ESP-IDF. "
            f"Set '{CONF_EXCLUDE_CERTIFICATE_BUNDLE}: true' to skip certificate validation."
        )

    return config


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


def validate_safe_mode(config):
    # using 'safe_mode' on 'esp8266' require 'restore_from_flash'
    if CORE.is_esp8266 and config[CONF_SAFE_MODE]:
        if not fv.full_config.get()["esp8266"]["restore_from_flash"]:
            raise cv.Invalid(
                "Using 'safe_mode' on 'esp8266' require 'restore_from_flash'."
                "See https://esphome.io/components/esp8266#configuration-variables"
            )
    return config


def _declare_request_class(value):
    if CORE.using_esp_idf:
        return cv.declare_id(OtaHttpRequestComponentIDF)(value)

    if CORE.is_esp8266 or CORE.is_esp32 or CORE.is_rp2040:
        return cv.declare_id(OtaHttpRequestComponentArduino)(value)
    return NotImplementedError


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): _declare_request_class,
            cv.Optional(
                CONF_TIMEOUT, default="5min"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_WATCHDOG_TIMEOUT): cv.All(
                cv.Any(cv.only_on_esp32, cv.only_on_rp2040),
                cv.positive_time_period_milliseconds,
            ),
            cv.SplitDefault(CONF_ESP8266_DISABLE_SSL_SUPPORT, esp8266=False): cv.All(
                cv.only_on_esp8266, cv.boolean
            ),
            cv.Optional(CONF_EXCLUDE_CERTIFICATE_BUNDLE, default=False): cv.boolean,
            cv.Optional(CONF_SAFE_MODE, default="fallback"): cv.Any(
                cv.boolean, "fallback"
            ),
            cv.Optional(CONF_MAX_URL_LENGTH, default=240): cv.uint16_t,
        }
    )
    .extend(BASE_OTA_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 5, 1),
        esp32_arduino=cv.Version(0, 0, 0),
        esp_idf=cv.Version(0, 0, 0),
        rp2040_arduino=cv.Version(0, 0, 0),
    ),
    validate_certificate_bundle,
)

FINAL_VALIDATE_SCHEMA = validate_safe_mode


@coroutine_with_priority(50.0)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await ota_to_code(var, config)
    cg.add(var.set_timeout(config[CONF_TIMEOUT]))
    cg.add_define("CONFIG_MAX_URL_LENGTH", config[CONF_MAX_URL_LENGTH])
    if (
        config.get(CONF_WATCHDOG_TIMEOUT, None)
        and config[CONF_WATCHDOG_TIMEOUT].total_milliseconds > 0
    ):
        cg.add_define(
            "CONFIG_WATCHDOG_TIMEOUT", config[CONF_WATCHDOG_TIMEOUT].total_milliseconds
        )

    if CORE.is_esp8266 and not config[CONF_ESP8266_DISABLE_SSL_SUPPORT]:
        cg.add_define("USE_HTTP_REQUEST_ESP8266_HTTPS")

    if CORE.is_esp32:
        if CORE.using_esp_idf:
            esp32.add_idf_sdkconfig_option(
                "CONFIG_MBEDTLS_CERTIFICATE_BUNDLE",
                not config.get(CONF_EXCLUDE_CERTIFICATE_BUNDLE),
            )
            esp32.add_idf_sdkconfig_option(
                "CONFIG_ESP_TLS_INSECURE",
                config.get(CONF_EXCLUDE_CERTIFICATE_BUNDLE),
            )
            esp32.add_idf_sdkconfig_option(
                "CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY",
                config.get(CONF_EXCLUDE_CERTIFICATE_BUNDLE),
            )
        else:
            cg.add_library("WiFiClientSecure", None)
            cg.add_library("HTTPClient", None)
    if CORE.is_esp8266:
        cg.add_library("ESP8266HTTPClient", None)
    if CORE.is_rp2040 and CORE.using_arduino:
        cg.add_library("HTTPClient", None)

    await cg.register_component(var, config)

    if config[CONF_SAFE_MODE]:
        if config[CONF_SAFE_MODE] is True:
            cg.add_define("OTA_HTTP_ONLY_AT_BOOT")
        cg.add(var.check_upgrade())


OTA_HTTP_REQUEST_ACTION_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.use_id(OtaHttpRequestComponent),
        cv.Required(CONF_MD5_URL): cv.templatable(validate_url),
        cv.Required(CONF_URL): cv.templatable(validate_url),
        cv.Optional(CONF_FORCE_UPDATE, default=False): cv.templatable(cv.boolean),
    }
)


OTA_HTTP_REQUEST_FLASH_ACTION_SCHEMA = automation.maybe_conf(
    CONF_URL,
    OTA_HTTP_REQUEST_ACTION_SCHEMA.extend(
        {
            cv.Optional(CONF_METHOD, default="flash"): cv.string,
        }
    ),
)


@automation.register_action(
    "ota_http_request.flash",
    OtaHttpRequestComponentFlashAction,
    OTA_HTTP_REQUEST_FLASH_ACTION_SCHEMA,
)
async def ota_http_request_action_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)

    template_ = await cg.templatable(config[CONF_MD5_URL], args, cg.std_string)
    cg.add(var.set_md5_url(template_))
    template_ = await cg.templatable(config[CONF_URL], args, cg.std_string)
    cg.add(var.set_url(template_))
    template_ = await cg.templatable(config[CONF_FORCE_UPDATE], args, cg.bool_)
    cg.add(var.set_force_update(template_))
    return var
